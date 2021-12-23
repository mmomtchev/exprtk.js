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

<!-- Generated by documentation.js. Update this documentation by updating the source code. -->

### Table of Contents

*   [Expression](#expression)
    *   [Parameters](#parameters)
    *   [Examples](#examples)
*   [expression](#expression-1)
*   [scalars](#scalars)
*   [type](#type)
*   [vectors](#vectors)
*   [cwise](#cwise)
    *   [Parameters](#parameters-1)
    *   [Examples](#examples-1)
*   [eval](#eval)
    *   [Examples](#examples-2)
*   [map](#map)
    *   [Parameters](#parameters-2)
    *   [Examples](#examples-3)
*   [reduce](#reduce)
    *   [Parameters](#parameters-3)
    *   [Examples](#examples-4)

## Expression

### Parameters

*   `expression` **[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)** function
*   `variables` **[Array](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Array)<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)>?** An array containing all the scalar variables' names, will be determined automatically if omitted, however order won't be guaranteed
*   `vectors` **Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), [number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number)>?** An object containing all the vector variables' names and their sizes, vector size must be known at compilation (object construction)

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

## expression

Return the expression as a string

Type: [string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)

## scalars

Return the scalar arguments as an array

Type: [Array](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Array)<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)>

## type

Return the type as a string

Type: [string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)

## vectors

Return the vector arguments as an object

Type: Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), [Array](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Array)<[number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number)>>

## cwise

Generic vector operation with implicit traversal

Supports automatic type conversions, multiple inputs and writing into a pre-existing array

### Parameters

*   `arguments` **Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), ([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>)>**&#x20;
*   `arguments` **...(([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>) | Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), ([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>)>)** of the function, iterator removed

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

Returns **[TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>**&#x20;

## eval

Evaluate the expression

### Examples

```javascript
// These two are equivalent
const r1 = expr.eval({a: 2, b: 5});  // less error-prone
const r2 = expr.eval(2, 5);          // slightly faster

// These two are equivalent
expr.evalAsync({a: 2, b: 5}, (e,r) => console.log(e, r));
expr.evalAsync(2, 5, (e,r) => console.log(e, r));
```

Returns **[number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number)**&#x20;

## map

Evaluate the expression for every element of a TypedArray

Evaluation and traversal happens entirely in C++ so this will be much
faster than calling array.map(expr.eval)

### Parameters

*   `array` **[TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>** for the expression to be iterated over
*   `iterator` **[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)** variable name
*   `arguments` **...(([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>) | Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), ([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>)>)** of the function, iterator removed

### Examples

```javascript
// Clamp values in an array to [0..1000]
const expr = new Expression('clamp(f, x, c)', ['f', 'x', 'c']);

// r will be a TypedArray of the same type
// These two are equivalent
const r1 = expr.map(array, 'x', 0, 1000);
const r2 = expr.map(array, 'x', {f: 0, c: 0});

expr.mapAsync(array, 'x', 0, 1000, (e,r) => console.log(e, r));
expr.mapAsync(array, 'x', {f: 0, c: 0}, (e,r) => console.log(e, r));
```

Returns **[TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>**&#x20;

## reduce

Evaluate the expression for every element of a TypedArray
passing a scalar accumulator to every evaluation

Evaluation and traversal happens entirely in C++ so this will be much
faster than calling array.reduce(expr.eval)

### Parameters

*   `array` **[TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>** for the expression to be iterated over
*   `iterator` **[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)** variable name
*   `accumulator` **[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String)** variable name
*   `initializer` **[number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number)** for the accumulator
*   `arguments` **...(([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>) | Record<[string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/String), ([number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number) | [TypedArray](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/TypedArray)\<T>)>)** of the function, iterator removed

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

Returns **[number](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global\_Objects/Number)**&#x20;

# Notes

## Integer types

Originally, `ExprTk` supports only floating point types. The version bundled with `ExprTk.js` has working integer support, but one should be extra careful as it internally uses `NaN` values and most built-in mathematical functions - like `sin`, `cos`, `pow` or `exp` - won't work correctly with integer types. Using unsigned types will further aggravate this. Always check the result of your function when using anything but basic arithmetic. Also, do not forget that the internal data type also applies to all eventual index variables - using an `ExprTk` `for` loop in an `eval()` over a large `Int8` array is not possible as the index variable won't be able to hold the index. Implicit `map()`, `reduce()` and `cwise()` loops are not affected by this as they use internal C++ variables that are not affected by the `Expression` type.

## Calling expressions from another addon

One of the main feature of `ExprTk.js` is allowing native C++ addons to accept expressions constructed in JS and evaluate them independently of the main V8 context.

You can check `test/addon.test.cc` for different examples of processing an `Expression` object received from JS and evaluating the expression. The calling JS code can be found in `test/capi.test.js`.

Be advised that, while being completely independent of V8, those function can still block if another asynchronous operation is running on that `Expression` object.

## Build time and binary size

`ExprTk` is a C++ template-based engine and it contains an exceptionally high number of symbols that are multiplied by the number of supported types. The final binary contains more than 250000 symbols which is the reason for the huge binary size and the slow build process. This has no effect on its performance or even its initial loading time as the symbols are not exported through the dynamic linker.
