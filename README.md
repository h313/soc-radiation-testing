# Software SEE Testing for Commodity SoCs
This tool detects single-event upsets and isolates them to errors in memory, cache, or the CPU
pipeline. The default configuration is meant for the Snapdragon 801 SoC on the Mars 2020 HBS, but
can be easily adapted to support other chips.

## Requirements
* CMake 3.16 or newer
* GCC/LLVM toolchain with C++14 support

## Building
```bash
mkdir build && cd build
cmake ..
make
```
