## Object Pool Memory Allocator

This is a C++11 implementation of an object pool memory allocator.

For more information on object pool allocators and their purpose see 
http://gameprogrammingpatterns.com/object-pool.html.

Both a fixed size pool and a dynamically growing pool implementation are
included. These are both implemented using the `MemoryPoolBlock`
class.

The main features of this implementation are:
* `MemoryPoolBlock` is a single allocation containing the MemoryPoolBlock
  instance, indices of used pool entries and the pool memory itself
* `new_object` method uses C++11 std::forward to pass construction arguments
  to the constructor of the new object being created in the pool
* `for_each` method will iterate over all live objects in the pool calling
  the given function on them
* `delete_all` method will free all pool objects at once, skipping the
  destructor call for trivial types

Occupancy is tracked using indexes into available entries in the block. The
MemoryPoolBlock keeps the next free index head. This index can be used to
find the next available block entry when allocating a new entry.

A separate list of indices is used to track occupancy versus reusing object
pool memory for this purpose to avoid polluting CPU caches with objects which
are deleted and thus no longer in use.

This class is not designed with exceptions in mind as most game code avoids
using exceptions.

## Unit Testing

Unit tests are written using the [Catch](https://github.com/philsquared/Catch)
unit testing framework. Unit tests are run through the `runtest` executable.

## Micro-benchmaking

This repository also includes `bench.hpp` which is a single header file
micro-benchmarking framework inspired by Catch and Rust's
[benchmarking tests](https://doc.rust-lang.org/book/benchmark-tests.html).

It's my intention to make this standalone at some point but at the
moment it's very preliminary implementation.

Currently each micro-benchmark compares the performance of the following:
* Fixed pool
* Dynamic pool with 64, 128 and 256 entry blocks
* The default allocator

Benchmarks output nanoseconds per iteration (lower is better) and megabytes per
second throughput (higher is better).

## Prerequisites

The test and benchmarking applications require [CMake](http://www.cmake.org) to
generate build files.

## Compiling and running

To generate a build, compile and run follow these steps:

~~~
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./runtests
./runbench
~~~

## License

This software is licensed under the zlib license, see the LICENSE file for
details.
