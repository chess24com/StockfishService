#include <jni.h>
#include <android/log.h>

#define LOG_TAG "StockfishServiceJNI"

#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , LOG_TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , LOG_TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , LOG_TAG,__VA_ARGS__)

#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <algorithm>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <streambuf>


// Stockfish
#include "bitboard.h"
#include "evaluate.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

namespace stockfishservice {

class engine_wrapper {
    engine_wrapper() { }

    ~engine_wrapper() {
        if (engine_thread.joinable()) {
            engine_thread.join();
        }
        if (service_object) {
            JNIEnv *jenv;
            vm->AttachCurrentThread(&jenv, NULL);
            jenv->DeleteGlobalRef(service_object);
            vm->DetachCurrentThread();
        }
    }

    void thread_loop();

public:
    static engine_wrapper &sharedEngine() {
        static engine_wrapper engineWrapper;
        return engineWrapper;
    }

    void startEngine();

    // delete copy and move constructors and assign operators
    engine_wrapper(engine_wrapper const &) = delete;             // Copy construct
    engine_wrapper(engine_wrapper &&) = delete;                  // Move construct
    engine_wrapper &operator=(engine_wrapper const &) = delete;  // Copy assign
    engine_wrapper &operator=(engine_wrapper &&) = delete;      // Move assign

    // ALL PUBLIC
    bool engine_is_running = false;
    std::mutex startup_mutex;
    std::condition_variable startup_cv;
    // output handling
    std::string output_line;
    jmethodID output_method = nullptr;
    jobject service_object = nullptr;
    JavaVM *vm = nullptr;
    // input handling
    std::deque<std::string> input_lines;
    std::thread engine_thread;
    std::mutex mutex;
    std::condition_variable condition_variable;
};

void engine_wrapper::startEngine() {
    engine_thread = std::thread(&engine_wrapper::thread_loop, this);
}

void engine_wrapper::thread_loop() {
    UCI::init(Options);
    PSQT::init();
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Search::init();
    Eval::init();
    Pawns::init();
    Threads.init();
    Tablebases::init(Options["SyzygyPath"]);
    TT.resize(Options["Hash"]);

    LOGE("going into UCI::loop");

    char *argv = "stockfish_engine";
    {
        UCI::loop(1, &argv);
    }

    Threads.exit();
    {
        std::lock_guard<std::mutex> lk(startup_mutex);
        LOGE("engine is stopped");
        engine_is_running = false;
    }
}

class jni_sink {
public:
    typedef char char_type;
    typedef boost::iostreams::sink_tag category;

    std::streamsize write(const char_type *s, std::streamsize n) {
        LOGE("at jni_sink");
        engine_wrapper &engine = engine_wrapper::sharedEngine();
        std::string tmp = std::string(s, n);
        std::size_t index = tmp.find("\n");
        if (index == std::string::npos) {
            engine.output_line += tmp;
            return n;
        }
        engine.output_line += tmp.substr(0, index);

        // send line to Java
        JNIEnv *jenv;
        engine.vm->AttachCurrentThread(&jenv, NULL);
        jstring args = jenv->NewStringUTF(engine.output_line.c_str());
        jenv->CallVoidMethod(engine.service_object, engine.output_method, args);
        jenv->DeleteLocalRef(args);
        engine.vm->DetachCurrentThread();
        engine.output_line = tmp.substr(index + 1);
        return n;
    }
};


class jni_source_boost : public boost::iostreams::source {
public:
    std::streamsize read(char_type *s, std::streamsize n) {
        LOGE("at jni_source");
        if (n == 0) {
            LOGE("requested streamsize is 0");
            return -1;
        }
        engine_wrapper &engine = engine_wrapper::sharedEngine();
        LOGE("jni_source waiting");
        std::unique_lock<std::mutex> lk(engine.mutex);
        LOGE("input mtx locked");
        if (!engine.engine_is_running) {
            {
                LOGE("engine was not running");
                std::lock_guard<std::mutex> lk(engine.startup_mutex);
                LOGE("startup mtx locked");
                engine.engine_is_running = true;
            }
            engine.startup_cv.notify_all();
            LOGE("startup waiters are notified");
        }
        LOGE("waiting for input");
        engine.condition_variable.wait(lk, [&] { return (engine.input_lines.size() > 0); });
        LOGE("jni_source got input");
        std::streamsize len = std::min((std::streamsize)engine.input_lines.front().length(), n);
        // std::streamsize len = engine.input_lines.front().length();
        LOGE("length of input : %lu", len);
        LOGE("input: %s", engine.input_lines.front().c_str());
        std::copy(engine.input_lines.front().begin(), engine.input_lines.front().begin() + len, s);
        if (len == engine.input_lines.front().length()) {
            engine.input_lines.pop_front();
            return len;
        }
        engine.input_lines.front() = engine.input_lines.front().substr(len);
        return len;
    }
};

} // namespace

// JNI
// ==================
// ==================

extern "C" {

boost::iostreams::stream_buffer<stockfishservice::jni_sink> sink_buf;
boost::iostreams::stream_buffer<stockfishservice::jni_source> source_buf;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *jenv;
    if (vm->GetEnv(reinterpret_cast<void **>(&jenv), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    jclass clazz = jenv->FindClass("de/cisha/stockfishservice/StockfishService");
    if (clazz == 0) {
        LOGE("class de/cisha/stockfish/StockfishService not found");
    }

    jmethodID engineToClient = jenv->GetMethodID(clazz, "engineToClient", "(Ljava/lang/String;)V");
    if (engineToClient == 0) {
        LOGE("method engineToClient not found");
    }
    LOGE("creating sharedEngine()");
    stockfishservice::engine_wrapper &engine = stockfishservice::engine_wrapper::sharedEngine();
    engine.output_method = engineToClient;
    engine.vm = vm;

    jenv->DeleteLocalRef(clazz);
    LOGE("redirecting cout and cin");

    std::cin.rdbuf(&source_buf);
    std::cout.rdbuf(&sink_buf);

    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL Java_de_cisha_stockfishservice_StockfishService_clientToEngine(JNIEnv *env,
                                                                                      jobject thiz,
                                                                                      jstring line) {
    LOGE("at clientToEngine");
    stockfishservice::engine_wrapper &engine = stockfishservice::engine_wrapper::sharedEngine();
    if (!engine.service_object) {
        LOGE("setting Java Service object");
        engine.service_object = env->NewGlobalRef(thiz);
    }
    if (!engine.engine_is_running) {
        LOGE("calling startEngine()");
        engine.startEngine();
        {
            std::unique_lock<std::mutex> ul(engine.startup_mutex);
            LOGE("waiting for the end of engine startup");
            engine.startup_cv.wait(ul, [&] {return engine.engine_is_running;});
            LOGE("engine startup wait is over");
        }
    }
    {
        jboolean is_copy;
        LOGE("getting input line from Java");
        const char *line_str = env->GetStringUTFChars(line, &is_copy);
        std::lock_guard<std::mutex> lk(engine.mutex);
        LOGE("inserting cmd line to queue");
        engine.input_lines.emplace_back(line_str);
        env->ReleaseStringUTFChars(line, line_str);
    }
    LOGE("waking up reader thread");
    engine.condition_variable.notify_one();
}

} // extern "C"



