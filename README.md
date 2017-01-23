# StockfishService
Stockfish packaged into a remote Android Service. For usage please go to [StockfishServiceTest](https://github.com/chess24com/StockfishServiceTest).

Embedded Stockfish's commit : **3ab3e55bb5faf57aec864f3bb7268601c11d72be**

## General information

This library enables the use of stockfish on Android.

It minimally patches the upstream code: 
* removed *main.cpp* (it's contents went to *stockfish-lib.cpp*)

This is an Android Library Module. It was designed to be easily added to any Android Studio project. It runs in its own process. Your code should communicate with it through a [Messenger](https://developer.android.com/reference/android/os/Messenger.html) class. It was designed to be started by *binding* not by calling *startService*. For details please check out the sample app [StockfishServiceTest](https://github.com/chess24com/StockfishServiceTest).

## Caveats

Stockfish was not designed to survive a *quit* uci command. It uses global variables and doesn't clean up after itself. Sending a *quit* command stops the engine, but you shouldn't call any commands after that. In case you want to restart the engine, just unbind and rebind to the remote service.

While it being a *remote* service prevents to take down your app in case stockfish crashes, due to an Android bug the user will still see a dialog saying your app crashed. You should vote for my bug report for this. TODO add bug report link.

Your Application's *onCreate()* (not Activity!) will be called for the remote service too, because each process has an associated Application object. You can differentiate between the two processes by checking for the current process's name. For details check out [StockfishServiceTest](https://github.com/chess24com/StockfishServiceTest).

## Details

Stockfish uses *std::cin* and *std::cout* to communicate. This is hardcoded. This wrapper library creates two specialized *std::streambuf* objects and redirects *std::cin* and *std::cout* to and from Java through JNI calls. In general your code should not deal with this at all. But it's good to know.

## Proguard

One java method is called only from C++ so that should be kept. Obfuscation should also be turned off for the Service class to not mess up the JNI lookups. Will update with a configuration snippet as soon as I will have time to test it.



