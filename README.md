# cppstdlib

A C++23 demo that parses a package/task configuration, validates it, detects dependency cycles, and prints reports.

## Requirements

- CMake 3.25+
- Ninja
- A C++23-capable compiler (GCC 13+ or Clang 17+)

## Build

Configure and build in debug mode:

```sh
cmake --preset debug
cmake --build --preset debug
```

Or release mode:

```sh
cmake --preset release
cmake --build --preset release
```

## Run

```sh
./build/debug/app
# or
./build/release/app
```

## Test

```sh
ctest --preset debug --output-on-failure
```
