# Node.js bindings for [ExprTk](http://www.partow.net/programming/exprtk/index.html)

[<img src="https://github.githubassets.com/images/modules/logos_page/GitHub-Mark.png" width="20"/>](https://github.com/mmomtchev/exprtk.js)
[![License: ISC](https://img.shields.io/github/license/mmomtchev/exprtk.js)](https://github.com/mmomtchev/exprtk.js/blob/master/LICENSE)
[![npm version](https://img.shields.io/npm/v/exprtk.js)](https://www.npmjs.com/package/exprtk.js)
[![Node.js CI](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-dev.yml/badge.svg)](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-dev.yml)
[![Test published packages](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-release.yml/badge.svg)](https://github.com/mmomtchev/exprtk.js/actions/workflows/test-release.yml)
[![codecov](https://codecov.io/gh/mmomtchev/exprtk.js/branch/main/graph/badge.svg?token=H8v2uuZGYg)](https://codecov.io/gh/mmomtchev/exprtk.js)

`ExprTk.js` supports both synchronous and asynchronous background execution of thunks precompiled from a string including asynchronous and multithreaded versions of `TypedArray.prototype.map` and `TypedArray.prototype.reduce` and a synchronous multi-threaded version of `TypedArray.prototype.map`.

Its main advantage is that it allows deferring of heavy computation for asynchronous execution in a background thread - something that Node.js/V8 does not allow without the very complex mechanisms of `worker_threads`.

Even in single-threaded synchronous mode `ExprTk.js` outperforms the native JS `TypedArray.prototype.map` running in V8 by a significant margin for all types and array sizes and it comes very close to a direct iterative `for` loop.

It also supports being directly called from native add-ons, including native threads, without any synchronization with V8, allowing JS code to drive multi-threaded mathematical transformations.

# Installation

```
npm install exprtk.js
```

# Usage

```
const Float64Expression = require('exprtk.js').Float64;

const expr = new Float64Expression('(a + b) / 2');
const inputArray = new Float64Array(1e6);

// Happens in a background thread, iterates over 'a', with b=4
const outputArray = await expr.mapAsync(inputArray, 'a', { 'b': 4 });

// or with 4 threads on 4 cores
const outputArray = await expr.mapAsync(4, inputArray, 'a', { 'b': 4 });
```

Refer to the [ExprTk manual](https://github.com/ArashPartow/exprtk/blob/master/readme.txt) for the full expression syntax.
