# ExprTk.js

This is a Node.js binding for [ExprTk](http://www.partow.net/programming/exprtk/index.html) [(Github)](https://github.com/ArashPartow/exprtk) by [@ArashPartow](https://github.com/ArashPartow)

It supports both synchronous and asynchronous background execution of thunks precompiled from a string including asynchronous and multithreaded versions of `TypedArray.prototype.map` and `TypedArray.prototype.reduce`

# Installation

ExprTk.js is a WIP and it is still not published

# Usage

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

Speedup is possible even when using synchronously if the arrays are sufficiently large. On 1MB arrays, `ExprTk.js` outperforms native JS running in V8 twice for floating point data and three times for integer data.

It can also serve as a provider of thunks for `gdal-async` and `scijs` allowing for easy multi-threaded processing in Node.js.

## Simple synchronous use

```js
const expr = require("exprtk.js").Float64;

// arithmetic mean
const mean = new expr('(a + b) / 2');
// clamp to a range
const clamp = new expr('clamp(minv, x, maxv)', ['minv', 'x', 'maxv']);
// sum n-powers
const sumPow = new expr('a + pow(x, n)', ['a', 'x', 'n']);

// arguments as a list
const m = mean.eval(2, 4);
const r = clamp.eval(5, 12, 10);

// arguments as an object
const r = clamp.eval({ a: 5, b: 10, x: 12 });


// map/reduce with C++ traversal
const inputArray = new Float64Array(n);

// these are equivalent
const resultingArray = clamp.map(inputArray, 'x', 5, 10);
const resultingArray = clamp.map(inputArray, 'x', {minv: 5, maxv: 10});

// these are equivalent
const sumSquares = sumPow.reduce(inputArray, 'x', 'a', 0, 2);
const sumSquares = sumPow.reduce(inputArray, 'x', 'a', 0, {p: 2});
```

## With a `TypedArray`

The data type and the array size  must be known when compiling the expression. `ExprTk` supports only fixed-size arrays.

```js
const expr = require("exprtk.js").Float64;

const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], { 'x': 6 });

const r = mean.eval(new Float64Array([ 1, 2, 3, 4, 5, 6 ])});
```

## Using asynchronously

When launching an asynchronous operation, the scalar arguments will be copied and any `TypedArray`s will be locked in place and protected from the GC. The whole operation, including traversal and evaluation will happen entirely in a pre-existing background thread from a pool and won't solicit the main thread until completion.

An `Expression` is not reentrant so multiple concurrent evaluations of the same object will wait on one another. Multiple evaluations on multiple objects will run in parallel up to the limit set by the Node.js environment variable `UV_THREADPOOL_SIZE`. Mixing synchronous and asynchronous evaluations is supported, but a synchronous evaluation will block the event loop until all asynchronous evaluations on that same object are finished.

```js
const expr = require("exprtk.js").Float64;

// Explicit traversal
const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], { 'x': 6 });

const r = await mean.evalAsync(new Float64Array([ 1, 2, 3, 4, 5, 6 ])});

// Implicit traversal
const clamp = new expr('clamp(minv, x, maxv)', ['minv', 'x', 'maxv']);

const resultingArray = await clamp.mapAsync(inputArray, 'x', 5, 10);

clamp.mapAsync(inputArray, 'x', 5, 10, (e,r) => console.log(e, r));


const sumPow = new expr('a + pow(x, n)', ['a', 'x', 'n']);
const sumSquares = await sumPow.reduceAsync(inputArray, 'x', 'a', 0, 2);
```

# Generic complex operations with `cwise()`/`cwiseAsync()`

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

# Integer types

Originally, `ExprTk` supports only floating point types. The version bundled with `ExprTk.js` has working integer support, but one should be extra careful as it internally uses `NaN` values and most built-in mathematical functions - like `sin`, `cos`, `pow` or `exp` - won't work correctly with integer types. Always check the result of your function when using anything but basic arithmetic.

# Notes

## Build time and binary size

`ExprTk` is a C++ template-based engine and it contains an exceptionally high number of symbols that are multiplied by the number of supported types. The final binary contains more than 250000 symbols which is the reason for the huge binary size and the slow build process. This has no effect on its performance or even its initial loading time as the symbols are not exported through the dynamic linker.
