# StockfishService
Stockfish packaged into a remote Android Service. For usage please see [StockfishServiceTest](https://github.com/chess24com/StockfishServiceTest).

Embedded Stockfish's commit : **8abb98455f6fa78092f650b8bae9c166f1b5a315**

## General information

This library enables the use of stockfish on Android.

It minimally patches the upstream code: 
* renamed *tbcore.cpp* -> *tbcore.hpp* (also in it's include references)
* removed *main.cpp* (it's contents went to *stockfish-lib.cpp*)

This is an Android Library Module. It was designed to be easily added to any Android Studio project. It runs in its own process. Your code should communicate with it through a [Messenger](https://developer.android.com/reference/android/os/Messenger.html) class. It was designed to be started by *binding* not by calling *startService*. For details please check out the sample app [StockfishServiceTest](https://github.com/chess24com/StockfishServiceTest).

## Caveats

Stockfish was not designed to survive a *quit* uci command. It doesn't clean up after itself. Sending a *quit* command stops the engine correctly, but you shouldn't call any commands after that. In case you want to kill the engine, just unbind and rebind to the remote service.

While it being a *remote* service prevents to take down your app in case stockfish crashes. Due to an Android bug the user will still see a dialog saying your app crashed. You should vote for my bug report for this. TODO add bug report link.

Your Application's *onCreate()* (not Activity!) will be called for the remote service too, because each process has an Application object. You can differentiate between the two processes by checking for the current process's name.

## Details

Stockfish uses *std::cin* and *std::cout* to communicate. This library creates two specialized *std::streambuf* objects and redirects *std::cin* and *std::cout*. These streambuf objects redirects the bytes to and from Java through JNI calls. In general you code should not deal with this at all.



