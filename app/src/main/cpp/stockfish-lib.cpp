#include <jni.h>
#include <android/log.h>

#define LOG_TAG "StockfishServiceJNI"

#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , LOG_TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , LOG_TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , LOG_TAG,__VA_ARGS__)

#include <string>
#include <iostream>
#include <thread>
#include <algorithm>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/stream_buffer.hpp>

// Stockfish
#include "bitboard.h"
#include "evaluate.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"


namespace stockfish {

class engine_wrapper {
    engine_wrapper() {}
    ~engine_wrapper() {
        if (engine_thread.joinable()) {
            engine_thread.join();
        }
        if (service_object) {
            JNIEnv* jenv;
            vm->AttachCurrentThread(&jenv, NULL);
            jenv->DeleteGlobalRef(service_object);
            vm->DetachCurrentThread();
        }
    }

    void thread_loop();
public:
    static engine_wrapper& sharedEngine() {
        static engine_wrapper engineWrapper;
        return engineWrapper;
    }

    void startEngine();

    // delete copy and move constructors and assign operators
    engine_wrapper(engine_wrapper const&) = delete;             // Copy construct
    engine_wrapper(engine_wrapper&&) = delete;                  // Move construct
    engine_wrapper& operator=(engine_wrapper const&) = delete;  // Copy assign
    engine_wrapper& operator=(engine_wrapper &&) = delete;      // Move assign

    // ALL PUBLIC
    bool engine_is_running = false;
    // output handling
    jmethodID output_method = nullptr;
    jobject service_object = nullptr;
    JavaVM* vm = nullptr;
    // input handling
    std::string input_line;
    std::thread engine_thread;
    std::mutex mutex;
    std::condition_variable condition_variable;
};

void engine_wrapper::startEngine() {
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

    engine_thread = std::thread(&engine_wrapper::thread_loop, this);
}

void engine_wrapper::thread_loop() {
    engine_is_running = true;

    char* argv = "stockfish_engine";

    UCI::loop(1, &argv);

    Threads.exit();
}

class jni_sink {
public:
    typedef char char_type;
    typedef boost::iostreams::sink_tag category;

    std::streamsize write(const char* s, std::streamsize n)
    {
        engine_wrapper& engine = engine_wrapper::sharedEngine();
        JNIEnv* jenv;
        engine.vm->AttachCurrentThread(&jenv, NULL);
        std::string tmp = std::string(s, n);
        jstring args = jenv->NewStringUTF(tmp.c_str());
        jenv->CallVoidMethod(engine.service_object, engine.output_method, args);
        jenv->DeleteLocalRef(args);
        engine.vm->DetachCurrentThread();
        return n;
    }
};

class jni_source {
public:
    typedef char char_type;
    typedef boost::iostreams::source_tag category;

    std::streamsize read(char* s, std::streamsize n)
    {
        engine_wrapper& engine = engine_wrapper::sharedEngine();
        std::unique_lock<std::mutex> lk(engine.mutex);
        engine.condition_variable.wait(lk, [&] {return (engine.input_line.length() > 0);});
        std::copy(engine.input_line.begin(), engine.input_line.end(), s);
        size_t len = engine.input_line.length();
        engine.input_line.clear();
        return len;
    }
};


// ==================
// JNI
// ==================

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* jenv;
    if (vm->GetEnv(reinterpret_cast<void**>(&jenv), JNI_VERSION_1_6) != JNI_OK) {
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

    engine_wrapper& engine = engine_wrapper::sharedEngine();
    engine.output_method = engineToClient;
    engine.vm = vm;

    jenv->DeleteLocalRef(clazz);

    boost::iostreams::stream_buffer<jni_sink> sink_buf;
    boost::iostreams::stream_buffer<jni_sink> source_buf;

    std::cin.rdbuf(&source_buf);
    std::cout.rdbuf(&sink_buf);

    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL Java_de_cisha_stockfishservice_StockfishService_clientToEngine(JNIEnv* env, jobject thiz, jstring line) {
    engine_wrapper& engine = engine_wrapper::sharedEngine();
    if (!engine.service_object) {
        engine.service_object = env->NewGlobalRef(thiz);
    }
    if (!engine.engine_is_running) {
        engine.startEngine();
    }
    {
        std::lock_guard<std::mutex> lk(engine.mutex);
        jboolean is_copy;
        const char* line_str = env->GetStringUTFChars(line, &is_copy);
        engine.input_line = std::string(line_str);
        env->ReleaseStringUTFChars(line, line_str);
    }
    engine.condition_variable.notify_one();
}




} // namespace



