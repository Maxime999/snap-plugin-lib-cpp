<!--
http://www.apache.org/licenses/LICENSE-2.0.txt


Copyright 2016 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

## Snap Plugin Library for C++ [![Build Status](https://travis-ci.org/intelsdi-x/snap-plugin-lib-cpp.svg?branch=master)](https://travis-ci.org/intelsdi-x/snap-plugin-lib-cpp)

This is a library for writing plugins in C++ for the [Snap telemetry framework](https://github.com/intelsdi-x/snap).

Snap has four different plugin types. For instructions on how to write a plugin check out the following links to example plugins:
* [collector](examples/collector/README.md),
* [streaming-collector](examples/streaming-collector/README.md).
* [processor](examples/processor/README.md),
* [publisher](examples/publisher/README.md).

Before writing a Snap plugin:

* See if one already exists in the [Plugin Catalog](https://github.com/intelsdi-x/snap/blob/master/docs/PLUGIN_CATALOG.md)
* See if someone mentioned it in the [plugin wishlist](https://github.com/intelsdi-x/snap/labels/plugin-wishlist)

If you do decide to write a plugin, open a new issue following the plugin [wishlist guidelines](https://github.com/intelsdi-x/snap/blob/master/docs/PLUGIN_CATALOG.md#wish-list) and let us know you are working on one!

## Brief Overview of Snap Architecture

Snap is an open and modular telemetry framework designed to simplify the collection, processing and publishing of data through a single HTTP based API. Plugins provide the functionality of collection, processing and publishing and can be loaded/unloaded, upgraded and swapped without requiring a restart of the Snap daemon.

A Snap plugin is a program that responds to a set of well defined [gRPC](http://www.grpc.io/) services with parameters and returns types specified as protocol buffer messages (see [plugin.proto](https://github.com/intelsdi-x/snap/blob/master/control/plugin/rpc/plugin.proto)). The Snap daemon handshakes with the plugin over stdout and then communicates over gRPC.

## Snap Plugin C++ Library Examples
You will find [example plugins](examples) that cover the basics for writing collector, processor, and publisher plugins in the examples folder.

## Building libsnap:

Requiered tools:
* autoconf
* automake
* curl
* g++
* libtool
* make

Dependencies:
* [Boost](http://www.boost.org) ([Github](https://github.com/boostorg/boost)), latest version (currently 1.68)
* [gRPC](https://grpc.io) and [Protobuf](https://developers.google.com/protocol-buffers/) ([Github](https://github.com/grpc/grpc)), latest version (currently 1.15.1)
* [cpp-netlib](http://cpp-netlib.org) ([Github](https://github.com/cpp-netlib/cpp-netlib)), latest version (currently 0.13.0)
* [spdlog](https://github.com/gabime/spdlog) ([Github](https://github.com/gabime/spdlog)), latest version (currently 1.2.0)
* [nlohmann json](https://github.com/nlohmann/json) ([Github](https://github.com/nlohmann/json)), latest version (currently 3.3.0)


### Boost
You can either download precompiled Boost libraries and headers or build them from source.

Versions 1.57 to latest (1.68) are supported.

Snap requires headers of Boost.Program_options, Boost.Algorithm, Boost.Any and Boost.Filesystem (the latter two may be replaced by their C++17 STL equivalents), and static libraries of `boost_program_options`, `boost_system` and `boost_filesystem`.

### gRPC and Protobuf
It is recommended to build gRPC and Protobuf from source to avoid dependency conflicts.

Versions 1.0.1 to latest (1.15.1) are supported.

#### To install GRPC
```bash
git clone https://github.com/grpc/grpc
cd grpc
git checkout tags/v1.15.1
git submodule update --init
make
[sudo] make install
```

#### To install Protobuf (Google Protocol Buffers):
Snap plugin library for C++ depends also on protobuf library. As it is already in gRPC dependencies, you can install it like:
```bash
cd grpc/third_party/protobuf
[sudo] make install
```

Then you have to generate Snap's plugin Protobuf interface:
```bash
cd snap-plugin-lib-cpp/src/snap/rpc
protoc --cpp_out=. plugin.proto
protoc --grpc_out=. --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin plugin.proto
```
**Be careful to use the `protoc` executable that was built previously!** It should be in `/usr/local/bin/`.


### cpp-netlib
Version 0.13.0 (latest) is supported.

```bash
git clone https://github.com/cpp-netlib/cpp-netlib
cd cpp-netlib
git checkout tags/cpp-netlib-0.12.0-final
git submodule update --init
```

Then, create a `cpp-netlib-build` directory next to the cloned repository:
```bash
cd ..
mkdir cpp-netlib-build
cd cpp-netlib-build
cmake ../cpp-netlib
make
[sudo] make install
```

### Asio
For cpp-netlib version 0.12.0, Asio headers need to be installed as well. This is not needed for other versions of cpp-netlib.

As Asio is already in cpp-netlib dependencies, you can install it like:
```bash
cd cpp-netlib/deps/asio/asio
./autogen.sh
./configure
make
[sudo] make install
```
And then build cpp-netlib again

Other versions of cpp-netlib use Boost.Asio, which is installed with Boost.

### spdlog
Versions 0.13.0 to latest (1.2.0) are supported.

```bash
git clone https://github.com/gabime/spdlog
cd spdlog
git checkout tags/v1.2.0
[sudo] cp -r ./include/spdlog /usr/local/include
```

### JSON
Versions 2.0.9 to latest (3.3.0) are supported.

```bash
wget https://github.com/nlohmann/json/releases/download/v3.3.0/json.hpp
[sudo] cp json.hpp /usr/local/include/json.hpp
```

### Once the above dependencies have been resolved:
First, execute `ldconfig /usr/local/lib/` to allow ld to look for installed libraries in `/usr/local/lib`. Then you can build the Snap library:
```bash
./autogen.sh
./configure
make
[sudo] make install
```

`libsnap` (and all previous dependencies) are installed into `/usr/local/lib`, which not all linkers use when searching for shared objects. Using the `--prefix=/usr` switch when running the `configure` script will place the resulting libraries into `/usr/lib`, for example.

To clean up and rebuild use:
```bash
make clean
git clean -xdf  # warning! This deletes all dirs and files not checked in. Be sure to check in any new files before running `git clean`.
```
