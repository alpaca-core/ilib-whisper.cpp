# Alpaca core whisper.cpp plugin

This is a wrapper of whisper.cpp implemented as per the discussion [Integration of llama.cpp and whisper.cpp](https://github.com/alpaca-core/alpaca-core/discussions/5):

* Use the whisper.cpp C interface
* Reimplement the common library

## Build

> [!IMPORTANT]
> When cloning this repo, don't forget to fetch the submodules.
> * Either: `$ git clone https://github.com/alpaca-core/ilib-whisper.cpp.git --recurse-submodules`
> * Or:
>    * `$ git clone https://github.com/alpaca-core/ilib-whisper.cpp.git`
>    * `$ cd ilib-whisper.cpp`
>    * `$ git submodule update --init --recursive`
