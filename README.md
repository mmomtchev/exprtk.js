# ExprTk.js

This is a Node.js binding for [ExprTk](http://www.partow.net/programming/exprtk/index.html) [(Github)](https://github.com/ArashPartow/exprtk) by @ArashPartow

It supports both synchronous and asynchronous background execution of thunks precompiled from a string

# Installation

ExprTk.js is a WIP and it is still not published

# Usage

ExprTk.js is only marginally faster, if faster at all, than executing the equivalent mathematical expression directly in Node.js/V8. Speedup when using synchronously is possible only for very large arrays.

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

It can also serve as a provider of thunks for `gdal-async` and `scijs` allowing for easy multi-threaded processing in Node.js.

## Simple synchronous use

```js
const expr = require("exprtk.js").Expression;

const mean = new expr('(a + b) / 2', ['a', 'b']);

// fn form
const r = mean.fn(5, 10);

// eval form
const r = mean.eval({ a: 5, b: 10 });
```

## With a `TypedArray`

Only `Float64` is currently supported. When using with a `TypedArray`, the expression must be recompiled when using with a new `TypedArray` as `ExprTk` supports only fixed-size arrays. The data in the `TypedArray` may be modified between evaluations.

```js
const expr = require("exprtk.js").Expression;

const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], {'x': new Float64Array([ 1, 2, 3, 4, 5, 6 ])});

const r = mean.fn();
```

## Using asynchronously

```js
const expr = require("exprtk.js").Expression;

const mean = new expr(
    'var r := 0;' + 
    'for (var i := 0; i < x[]; i += 1)' +
    '{ r += x[i]; };' +
    'r / x[];',
    [], {'x': new Float64Array([ 1, 2, 3, 4, 5, 6 ])});

const r = await mean.fnAsync();
```