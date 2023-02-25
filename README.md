# Software SEE Testing for Commodity SoCs
This tool detects single-event upsets and isolates them to errors in memory, cache, or the CPU
pipeline. The default configuration is meant for the Snapdragon 801 SoC on the Mars 2020 HBS, but
can be easily adapted to support other chips.

## Usage
```bash
radiation_test memory_size
```

## Requirements
* CMake 3.16 or newer
* GCC/LLVM toolchain with C++14 support

## Building
```bash
mkdir build && cd build
cmake -G Ninja ..
make
```

## Design
The program reserves `memory_size` megabytes of memory using `malloc` and writes a repeating pattern
of `0b10101010` to the reserved locations. Two threads are then spawned to test the integrity of the
memory locations, which allows us to differentiate between faults in the L1 cache and in RAM. As the
L2 cache has SECDED ECC, we assume that SEEs will not affect data there.

After reading a block of memory that takes up a portion of the cache defined by `L1_USAGE`, each
thread will run a set of tests on the program twice. These tests will exercise the CPU's ALU and
multiply-add pipelines.

## Possible Errors
* Incorrect cache read on two cores: potential single-event upset in RAM
* Incorrect cache read on one core: potential single-event upset in L1 cache
* Incorrect ALU/multiply-add result: potential single-event transient in CPU pipeline
