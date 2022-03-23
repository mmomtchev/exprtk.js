# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] WIP
 - Switch to C++17
 - Support strided N-dimensional arrays in `cwise`/`cwiseAsync`

### [2.0.2] WIP
 - Fix #31, race condition in `cwise` MP mode

### [2.0.1] 2022-03-22
 - Fix baseline N-API to version 4 allowing extending support to akk Node.js >=10.16.0
 - Fix #28, validate the length of the target array in `cwise`/`cwiseAsync`
 - Fix #20, minor memory leak when the `Expression()` constructor throws on invalid vector name
 - Fix #30, impose a limit for vector sizes when using integer `Expression` types

# [2.0.0] 2022-02-14
 - Full reentrancy, supporting multiple concurrent evaluations of the same `Expression` object
 - New multithreading model independent of Node.js/libuv
 - Support OpenMP-style parallelism in `map`/`mapAsync` and `cwise`/`cwiseAsync`
 - Support writing into a preallocated array for `Expression.prototype.map`
 - Support passing `TypedArray` subarrays (`TypedArray.prototype.subarray`)
 - `Expression.prototype.toString()`
 - Add a shared superclass `Expression` supporting testing with `instanceof`
 - `Expression.prototype.allocator()` allowing to call the TypedArray constructor

### [1.0.1] 2022-01-03
 - Fix missing promisification of the async methods for integer types
 - Correct wrong exception message

# [1.0.0] 2021-12-26
 - Fix missing TS types
 - Minor README.md updates
 - Add `documentation-polyglot` as a dev dependency
 - Add a console warning if the GC tries to free a locked object

## [1.0.0-rc1] 2021-12-24
 - First binary release
