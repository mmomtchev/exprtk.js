# ExprTk.js

This is a Node.js binding for [ExprTk](http://www.partow.net/programming/exprtk/index.html) [(Github)](https://github.com/ArashPartow/exprtk) by [@ArashPartow](https://github.com/ArashPartow)

It supports both synchronous and asynchronous background execution of thunks precompiled from a string including asynchronous and multithreaded versions of `TypedArray.prototype.map` and `TypedArray.prototype.reduce`

# Installation

ExprTk.js is a WIP and it is still not published

# Usage

ExprTk.js is only marginally faster, if faster at all, than executing the equivalent mathematical expression directly in Node.js/V8. Speedup when using synchronously is possible only for very large arrays.

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

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

// map with C++ traversal
const inputArray = new Float64Array(n);
const resultingArray = clamp.map(inputArray, 'x', 5, 10);
const sumSquares = sumPow.reduce(inputArray, 'x', 'a', 0, 2);
```

## With a `TypedArray`

Only `Float64` and `Float32` are supported. As `ExprTk` supports only fixed-size arrays the size must be known when compiling the expression.

```js
const expr = require("exprtk.js").Expression;

const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], { 'x': 6 });

const r = mean.eval(new Float64Array([ 1, 2, 3, 4, 5, 6 ])});
```

## Using asynchronously

An `Expression` is not reentrant so multiple concurrent evaluations of the same object will wait on one another. Multiple evaluations on multiple objects will run in parallel up to the limit set by the Node.js environment variable `UV_THREADPOOL_SIZE`. Mixing synchronous and asynchronous evaluations is supported, but a synchronous evaluation will block the event loop until all asynchronous evaluations on that same object are finished.

```js
const expr = require("exprtk.js").Expression;

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

# Integer types

`ExprTk` currently supports only floating point types. If you can get Mr. [@ArashPartow](https://github.com/ArashPartow) to add support for integer types, releasing a compatible `ExprTk.js` will be a matter of a few hours of work. Adding integer support to `ExprTk` is definitely not a trivial task and I have no need for it - but I will gladly accept funding to implement it.
