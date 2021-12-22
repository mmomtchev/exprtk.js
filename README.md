# ExprTk.js

This is a Node.js binding for [ExprTk](http://www.partow.net/programming/exprtk/index.html) [(Github)](https://github.com/ArashPartow/exprtk) by [@ArashPartow](https://github.com/ArashPartow)

It supports both synchronous and asynchronous background execution of thunks precompiled from a string including asynchronous and multithreaded versions of `TypedArray.prototype.map` and `TypedArray.prototype.reduce`.

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

Even in single-threaded synchronous mode `ExprTk.js` outperforms the native JS `TypedArray.prototype.map` running in V8 by a significant margin for all types and array sizes and it comes very close to a direct iterative `for` loop.

It can also serve as a provider of thunks for `gdal-async` and `scijs` allowing for easy multi-threaded processing in Node.js.

# Installation

ExprTk.js is a WIP and it is still not published

# Usage

Different methods of traversal work better for different array sizes, you should probably run/adjust the benchmarks - `npm run bench` - to see for yourself.

For very small array sizes, the fast setup of `eval`/`evalAsync` will be better. For large arrays, the tight internal loop of `map()`/`mapAsync()` will be better.

When launching a large number of parallel operations, unless the expression is very complex, the bottleneck will be the cache/memory bandwidth.

The original documentation of `ExprTk` and the syntax used for the expressions is available here: <https://github.com/ArashPartow/exprtk>

## Usage

When launching an asynchronous operation, the scalar arguments will be copied and any `TypedArray`s will be locked in place and protected from the GC. The whole operation, including traversal and evaluation will happen entirely in a pre-existing background thread picked from a pool and won't solicit the main thread until completion.

An `Expression` is not reentrant so multiple concurrent evaluations of the same `Expression` object will wait on one another. Multiple evaluations on multiple objects will run in parallel up to the limit set by the Node.js environment variable `UV_THREADPOOL_SIZE`. Mixing synchronous and asynchronous evaluations is supported, but a synchronous evaluation will block the event loop until all asynchronous evaluations on that same object are finished.

Support for a reentrant `MPExpression` that can distribute its array over multiple threads is planned for the next version.

### Simple synchronous example

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
## Generic vector operations with `cwise()`/`cwiseAsync()`

`cwise()` and `cwiseAsync()` allow for generic coefficient-wise operations on multiple vectors with implicit array traversal.

These are the only methods that support type conversions and writing into preexisting arrays.

```js
// Air density of humid air from relative humidity (φ), temperature (T) and pressure (P)
// rho = ( Pd * Md + Pv * Mv ) / ( R * T )    // density (Avogadro's law)
// Pv = φ * Ps                                // vapor pressure of water
// Ps = 6.1078 * 10 ^ (7.5 * T / (T + 237.3)) // saturation vapor pressure (Tetens' equation)
// Pd = P - Pv                                // partial pressure of dry air
// R = 0.0831446                              // universal gas constant
// Md = 0.0289652                             // molar mass of water vapor
// Mv = 0.018016                              // molar mass of dry air
// ( this is the weather science form of the equation and not the hard physics one
//   with T in C° and pressure in hPa )
// phi, T and P are arbitrary TypedArrays of the same size

// Calculation uses Float64 internally
const expr = require("exprtk.js").Float64;

const density = new expr(
    // compute Pv and store it
    'var Pv := ( phi * 6.1078 * pow(10, (7.5 * T / (T + 237.3))) ); ' +
    // main formula (and return expression)
    '( (P - Pv) * Md + Pv * Mv ) / ( R * (T + 273.15) )',
    ['P', 'T', 'phi', 'R', 'Md', 'Mv']
);
const R = 0.0831446;
const Md = 0.0289652;
const Mv = 0.018016;
// Mixing array types
const fi = new Float32Array([0, 0.2, 0.5, 0.9, 0.5]);
const P = new Uint16Array([1013, 1013, 1013, 1013, 995]);
const T = new Uint16Array([25, 25, 25, 25, 25]);

// Preexisting array, type conversion is automatic
const result = new Float32Array(5);

// sync
const r = density.cwise({ P, T, fi, R, Mv, Md }, result);

// async
const r = await density.cwiseAsync({ P, T, fi, R, Mv, Md }, result);
```

# API

## Supported types

---
| JS | C/C++ |
| --- | --- |
| Float64 | double |
| Float32 | float |
| Uint32 | uint32_t (unsigned long) |
| Int32 | int32_t (long) |
| Uint16 | uint16_t (unsigned short) |
| Int16 | int16_t (short) |
| Uint8 | uint8_t (unsigned char) |
| Int8 | int8_t (char) |

### Integer types

Originally, `ExprTk` supports only floating point types. The version bundled with `ExprTk.js` has working integer support, but one should be extra careful as it internally uses `NaN` values and most built-in mathematical functions - like `sin`, `cos`, `pow` or `exp` - won't work correctly with integer types. Always check the result of your function when using anything but basic arithmetic.

## `Expression` object

The `Expression` represents an expression compiled to an AST from a string. Expressions come in different flavors depending on the internal type used.

```js
const expr = require("exprtk.js").Float64;
const sumPow = new expr('a + pow(x, n) + v[0] + v[1]',
                        ['a', 'x', 'n'], {v: 2});
```

The input argument list must be given explicitly. `ExprTk` supports only fixed-size arrays, so the size must be known at compilation (ie `Expression` object construction).

## `eval()/evalAsync()`

`eval()` simply calls the expression with values corresponding to every input argument.


# Notes

## Build time and binary size

`ExprTk` is a C++ template-based engine and it contains an exceptionally high number of symbols that are multiplied by the number of supported types. The final binary contains more than 250000 symbols which is the reason for the huge binary size and the slow build process. This has no effect on its performance or even its initial loading time as the symbols are not exported through the dynamic linker.
