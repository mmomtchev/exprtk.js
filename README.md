# ExprTk.js

[![License: ISC](https://img.shields.io/github/license/mmomtchev/exprtk.js)](https://github.com/mmomtchev/exprtk.js/blob/master/LICENSE)
[![npm version](https://img.shields.io/npm/v/exprtk.js)](https://www.npmjs.com/package/exprtk.js)
[![Node.js CI](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-dev.yml/badge.svg)](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-dev.yml)
[![Test published packages](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-release.yml/badge.svg)](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-release.yml)
[![codecov](https://codecov.io/gh/mmomtchev/exprtk.js/branch/main/graph/badge.svg?token=H8v2uuZGYg)](https://codecov.io/gh/mmomtchev/exprtk.js)

This is the Node.js bindings for [ExprTk](http://www.partow.net/programming/exprtk/index.html) [(Github)](https://github.com/ArashPartow/exprtk) by [@ArashPartow](https://github.com/ArashPartow)

`ExprTk.js` supports both synchronous and asynchronous background execution of thunks precompiled from a string including asynchronous and multithreaded versions of `TypedArray.prototype.map` and `TypedArray.prototype.reduce` and a synchronous multi-threaded version of `TypedArray.prototype.map`.

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

Even in single-threaded synchronous mode `ExprTk.js` outperforms the native JS `TypedArray.prototype.map` running in V8 by a significant margin for all types and array sizes and it comes very close to a direct iterative `for` loop.

It also supports being directly called from native add-ons, including native threads, without any synchronization with V8, allowing JS code to drive multi-threaded mathematical transformations.

# Installation

`ExprTk.js` uses `node-pre-gyp` and it comes with pre-built binaries for x86-64 for Linux (baseline is Ubuntu 18.04), Windows and OS X.

```bash
npm install exprtk.js
```

If your platform is not supported, you can rebuild the binaries:

```bash
npm install exprtk.js --build-from-source
```

Rebuilding requires a working C++14 environment. It has been tested with `g++`, `clang` and `MSVC 2019`.

# Usage

Different methods of traversal work better for different array sizes, you should probably run/adjust the benchmarks - `npm run bench` - to see for yourself.

For very small array sizes, the fast setup of `eval`/`evalAsync` will be better. For large arrays, the tight internal loop of `map()`/`mapAsync()` will be better.

When launching a large number of parallel operations, unless the expression is very complex, the bottleneck will be the cache/memory bandwidth.

The original documentation of `ExprTk` and the syntax used for the expressions is available here: <https://github.com/ArashPartow/exprtk>

When launching an asynchronous operation, the scalar arguments will be copied and any `TypedArray`s will be locked in place and protected from the GC. The whole operation, including traversal and evaluation will happen entirely in a pre-existing background thread picked from a pool and won't solicit the main thread until completion.

## Parallelism

### 1.0

An `Expression` is not reentrant so multiple concurrent evaluations of the same `Expression` object will wait on one another. This is something that is taken care by the module itself - whether it is called from JS or from C/C++. Multiple evaluations on multiple `Expression` objects will run in parallel up to the limit set by the Node.js environment variable `UV_THREADPOOL_SIZE`. Mixing synchronous and asynchronous evaluations is supported, but a synchronous evaluation will block the event loop until all asynchronous evaluations on that same object are finished. If an evaluation of an `Expression` object has to wait for a previous evaluation of the same object to complete, the two evaluations will use two thread pool slots. This means that starting `UV_THREADPOOL_SIZE` evaluations on a single object can tie down the whole thread pool until the first one is completed.

### 2.0 (upcoming)

A single `Expression` object can contain multiple `ExprTk` `expression` instances that are compiled on-demand when needed up to a limit set by the `maxParallel` instance property. The global number of available threads can be set by using the environment variable `EXPRTKJS_THREADS` and it is independent of Node.js/libuv's own async work mechanism. It can be read from the `maxParallel` static class property. The actual peak instances usage of an `Expression` object can be checked by reading the `maxActive` instance property.

## Simple examples

```js
// internal type will be Float64 (C++ double)
const expr = require("exprtk.js").Float64;

// arithmetic mean
const mean = new expr('(a + b) / 2');

const m = mean.eval({a: 2, b: 4});
```

### Array traversal with `map()`/`mapAsync()`

```js
const inputArray = new Float64Array(n);
// clamp to a range
const clamp = new expr('clamp(minv, x, maxv)', ['minv', 'x', 'maxv']);

// these are equivalent
const resultingArray = clamp.map(inputArray, 'x', 5, 10);
const resultingArray = clamp.map(inputArray, 'x', {minv: 5, maxv: 10});

// async
const resultingArray = await clamp.mapAsync(inputArray, 'x', 5, 10);
const resultingArray = await clamp.mapAsync(inputArray, 'x', {minv: 5, maxv: 10});

// OpenMP-style (4 threads map)
const resultingArray = clamp.map(4, inputArray, 'x', 5, 10);
const resultingArray = await clamp.mapAsync(4, inputArray, 'x', {minv: 5, maxv: 10});

```

### Array traversal with `reduce()`/`reduceAsync()`

```js
const inputArray = new Float64Array(n);
// sum n-powers
const sumPow = new expr('a + pow(x, n)', ['a', 'x', 'n']);
// these are equivalent
const sumSquares = sumPow.reduce(inputArray, 'x', 'a', 0, 2);
const sumSquares = sumPow.reduce(inputArray, 'x', 'a', 0, {p: 2});

// async
const sumSquares = await sumPow.reduceAsync(inputArray, 'x', 'a', 0, 2);
const sumSquares = await sumPow.reduceAsync(inputArray, 'x', 'a', 0, {p: 2});
```

## Explicit traversal by using an ExprTk `for` loop

The data type and the array size must be known when compiling (constructing) the expression. `ExprTk` supports only fixed-size arrays.

```js
const inputArray = new Float64Array(n);
const expr = require("exprtk.js").Float64;

const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], { 'x': 6 });

const r = mean.eval(inputArray);
const r = await mean.evalAsync(inputArray);
```

# API

<!-- Generated by documentation.js. Update this documentation by updating the source code. -->

### Table of Contents

*   [toString](#tostring)
*   [expression](#expression)
*   [maxActive](#maxactive)
*   [maxParallel](#maxparallel)
*   [scalars](#scalars)
*   [type](#type)
*   [vectors](#vectors)
*   [cwise](#cwise)
    *   [Parameters](#parameters)
    *   [Examples](#examples)
*   [eval](#eval)
    *   [Parameters](#parameters-1)
    *   [Examples](#examples-1)
*   [map](#map)
    *   [Parameters](#parameters-2)
    *   [Examples](#examples-2)
*   [reduce](#reduce)
    *   [Parameters](#parameters-3)
    *   [Examples](#examples-3)
*   [maxParallel](#maxparallel-1)
*   [Expression](#expression-1)
    *   [Parameters](#parameters-4)
    *   [Examples](#examples-4)

## toString

Get a string representation of this object

Returns **string**&#x20;

## expression

Return the expression as a string

Type: string

## maxActive

Get the currently reached peak of simultaneously running instances for this Expression

Type: number

## maxParallel

Get/set the maximum allowed parallel instances for this Expression

Type: number

## scalars

Return the scalar arguments as an array

Type: Array\<string>

## type

Return the type as a string

Type: string

## vectors

Return the vector arguments as an object

Type: Record\<string, Array\<number>>

## cwise

Generic vector operation with implicit traversal.

Supports automatic type conversions, multiple inputs and writing into a pre-existing array.

### Parameters

*   `arguments` **Record\<string, (number | TypedArray\<T>)>**&#x20;
*   `arguments` **...(Array<(number | TypedArray\<T>)> | Record\<string, (number | TypedArray\<T>)>)** of the function, iterator removed

### Examples

```javascript
// Air density of humid air from relative humidity (phi), temperature (T) and pressure (P)
// rho = ( Pd * Md + Pv * Mv ) / ( R * (T + 273.15) // density (Avogadro's law)
// Pv = phi * Ps                                    // vapor pressure of water
// Ps = 6.1078 * 10 ^ (7.5 * T / (T + 237.3))       // saturation vapor pressure (Tetens' equation)
// Pd = P - Pv                                      // partial pressure of dry air
// R = 0.0831446                                    // universal gas constant
// Md = 0.0289652                                   // molar mass of water vapor
// Mv = 0.018016                                    // molar mass of dry air
// ( this is the weather science form of the equation and not the hard physics one with T in C° )
// phi, T and P are arbitrary TypedArrays of the same size
//
// Calculation uses Float64 internally
// Result is stored in Float32

const R = 0.0831446;
const Md = 0.0289652;
const Mv = 0.018016;
// cwise()/cwiseAsync() accept and automatically convert all data types
const phi = new Float32Array([0, 0.2, 0.5, 0.9, 0.5]);
const P = new Uint16Array([1013, 1013, 1013, 1013, 995]);
const T = new Uint16Array([25, 25, 25, 25, 25]);

const density = new Float64Expression(
  'Pv := ( phi * 6.1078 * pow(10, (7.5 * T / (T + 237.3))) ); ' +  // compute Pv and store it
  '( (P - Pv) * Md + Pv * Mv ) / ( R * (T + 273.13) )',            // return expression
   ['P', 'T', 'phi', 'R', 'Md', 'Mv']
);
const result = new Float32Array(P.length);

// sync
density.cwise({phi, T, P, R, Md, Mv}, result);

// async
await density.cwiseAsync({phi, T, P, R, Md, Mv}, result);
```

Returns **TypedArray\<T>**&#x20;

## eval

Evaluate the expression.

All arrays must match the internal data type.

### Parameters

*   `arguments` **...(Array<(number | TypedArray\<T>)> | Record\<string, (number | TypedArray\<T>)>)** of the function

### Examples

```javascript
// These two are equivalent
const r1 = expr.eval({a: 2, b: 5});  // less error-prone
const r2 = expr.eval(2, 5);          // slightly faster

// These two are equivalent
expr.evalAsync({a: 2, b: 5}, (e,r) => console.log(e, r));
expr.evalAsync(2, 5, (e,r) => console.log(e, r));
```

Returns **number**&#x20;

## map

Evaluate the expression for every element of a TypedArray.

Evaluation and traversal happens entirely in C++ so this will be much
faster than calling `array.map(expr.eval)`.

All arrays must match the internal data type.

If target is specified, it will write the data into a preallocated array.
This can be used when multiple operations are chained to avoid reallocating a new array at every step.
Otherwise it will return a new array.

### Parameters

*   `threads` **TypedArray\<T>?** number of threads to use, 1 if not specified
*   `target` **TypedArray\<T>?** array in which the data is to be written, will allocate a new array if none is specified
*   `array` **TypedArray\<T>** for the expression to be iterated over
*   `iterator` **string** variable name
*   `arguments` **...(Array<(number | TypedArray\<T>)> | Record\<string, (number | TypedArray\<T>)>)** of the function, iterator removed

### Examples

```javascript
// Clamp values in an array to [0..1000]
const expr = new Expression('clamp(f, x, c)', ['f', 'x', 'c']);

// In a preallocated array
const r = new array.constructor(array.length);
// These two are equivalent
expr.map(r, array, 'x', 0, 1000);
expr.map(r, array, 'x', {f: 0, c: 0});

expr.mapAsync(r, array, 'x', 0, 1000, (e,r) => console.log(e, r));
expr.mapAsync(r, array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));

// In a new array
// r1/r2 will be TypedArray's of the same type
const r1 = expr.map(array, 'x', 0, 1000);
const r2 = expr.map(array, 'x', {f: 0, c: 0});

expr.mapAsync(array, 'x', 0, 1000, (e,r) => console.log(e, r));
expr.mapAsync(array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));

// Using 4 multiple parallel threads (OpenMP-style parallelism)
const r1 = expr.map(4, array, 'x', 0, 1000);
const r2 = expr.map(4, array, 'x', {f: 0, c: 0});
```

Returns **TypedArray\<T>**&#x20;

## reduce

Evaluate the expression for every element of a TypedArray
passing a scalar accumulator to every evaluation.

Evaluation and traversal happens entirely in C++ so this will be much
faster than calling `array.reduce(expr.eval)`.

All arrays must match the internal data type.

### Parameters

*   `array` **TypedArray\<T>** for the expression to be iterated over
*   `iterator` **string** variable name
*   `accumulator` **string** variable name
*   `initializer` **number** for the accumulator
*   `arguments` **...(Array<(number | TypedArray\<T>)> | Record\<string, (number | TypedArray\<T>)>)** of the function, iterator removed

### Examples

```javascript
// n-power sum of an array
const sum = new Expression('a + pow(x, p)', ['a', 'x', 'p']);

// sumSq will be a scalar number

// These are equivalent
const sumSq = sum.reduce(array, 'x', 'a', 0, {'p': 2});
const sumSq = sum.reduce(array, 'x', 'a', 0, 2);

sum.reduceAsync(array, 'x', {'a' : 0}, (e,r) => console.log(e, r));
const sumSq = await sum.reduceAsync(array, 'x', {'a' : 0}, (e,r) => console.log(e, r));
```

Returns **number**&#x20;

## maxParallel

Get the number of threads available for evaluating expressions.
Set by the `EXPRTKJS_THREADS` environment variable.

Type: number

## Expression

### Parameters

*   `expression` **string** function
*   `variables` **Array\<string>?** An array containing all the scalar variables' names, will be determined automatically if omitted, however order won't be guaranteed, scalars are passed by value
*   `vectors` **Record\<string, number>?** An object containing all the vector variables' names and their sizes, vector size must be known at compilation (object construction), vectors are passed by reference and can be modified by the ExprTk expression

### Examples

```javascript
// This determines the internally used type
const expr = require("exprtk.js").Float64;

// arithmetic mean of 2 variables
const mean = new Expression('(a+b)/2', ['a', 'b']);

// naive stddev of an array of 1024 elements
const stddev = new Expression(
 'var sum := 0; var sumsq := 0; ' + 
 'for (var i := 0; i < x[]; i += 1) { sum += x[i]; sumsq += x[i] * x[i] }; ' +
 '(sumsq - (sum*sum) / x[]) / (x[] - 1);',
 [], {x: 1024})
```

Returns **[Expression](#expression)**&#x20;

# Notes

## Integer types

Originally, `ExprTk` supports only floating point types. The version bundled with `ExprTk.js` has working integer support, but one should be extra careful as it internally uses `NaN` values and most built-in mathematical functions - like `sin`, `cos`, `pow` or `exp` - won't work correctly with integer types. Using unsigned types will further aggravate this. Always check the result of your function when using anything but basic arithmetic. Also, do not forget that the internal data type also applies to all eventual index variables - using an `ExprTk` `for` loop in an `eval()` over a large `Int8` array is not possible as the index variable won't be able to hold the index. Implicit `map()`, `reduce()` and `cwise()` loops are not affected by this as they use internal C++ variables that are not affected by the `Expression` type.

## Evaluating expressions from another addon

One of the main features of `ExprTk.js` is allowing native C++ addons to accept expressions constructed in JS and then to evaluate them independently of the main V8 context.

You can check `test/addon.test.cc` for different examples of processing an `Expression` object received from JS and evaluating the expression. The calling JS code can be found in `test/capi.test.js`.

You will need `ExprTk.js` as a development dependency and you will need to include `node_modules/exprtk.js/include/exprtkjs.h` in your C/C++ code. You don't need to link against anything and your package won't require `ExprTk.js` as a production dependency. If the package is installed and the JS code sends you an `Expression` object, everything that will be needed to decode it and to evaluate the expression will be contained in it.

You will need to be much more careful when using the C-API which is much less strict on checking its input arguments. Passing dangling pointers or arrays of incorrect sizes or types will result in a Node.js crash.

Be also advised that, while being completely independent of V8, the C-API can still block if all `Expression` instances are busy running asynchronous operations. The module is always safe to call and it will take care of waiting for all other operations to complete.

Invocations from C++ follow the synchronous call semantics as it is expected that a C++ module will manage its own threads. Normally, one should avoid calling the module from the main thread - as this will block the event loop until the evaluation completes.

## Build time and binary size

`ExprTk` is a C++ template-based engine and it contains an exceptionally high number of symbols that are multiplied by the number of supported types. The final binary contains more than 250000 symbols which is the reason for the huge binary size and the slow build process. This has no effect on its performance or even its initial loading time as the symbols are not exported through the dynamic linker.

## Security

`ExprTk` is a Turing-complete evaluator that is not properly sandboxed from a security point of view, so untrusted user input is not to be used in an `Expression`.
