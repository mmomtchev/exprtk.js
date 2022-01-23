const path = require('path');
const binary = require('@mapbox/node-pre-gyp');
const binding_path = binary.find(path.resolve(path.join(__dirname, '../package.json')));
const addon = require(binding_path);

const promisify = require('util').promisify;

const types = [
    'Int8',
    'Uint8',
    'Int16',
    'Uint16',
    'Int32',
    'Uint32',
    'Float32',
    'Float64'
];

const promisifiables = [
    'evalAsync',
    'mapAsync',
    'reduceAsync',
    'cwiseAsync'
];

/**
 * Evaluate the expression for every element of a TypedArray
 * distributing the array over multiple threads.
 *  
 * All arrays must match the internal data type.
 * 
 * If target is specified, it will write the data into a preallocated array.
 * This can be used when multiple operations are chained to avoid reallocating a new array at every step.
 * Otherwise it will return a new array.
 *
 * @param {TypedArray<T>} [target] array in which the data is to be written
 * @param {TypedArray<T>} array for the expression to be iterated over
 * @param {number} threads number, must not exceed Expression.maxParallel
 * @param {string} iterator variable name
 * @param {...(number|TypedArray<T>)[]|Record<string, number|TypedArray<T>>} arguments of the function, iterator removed
 * @returns {TypedArray<T>}
 * @memberof Expression
 *
 * @example
 * // Clamp values in an array to [0..1000]
 * const expr = new Expression('clamp(f, x, c)', ['f', 'x', 'c']);
 *
 * await expr.mapMPAsync(array, expr.maxParallel, 'x', 0, 1000);
 */
function mapMPAsync() {
    let array, target, threads, parameters;

    if (typeof arguments[0] === 'object' && typeof arguments[0].length === 'number' &&
        typeof arguments[1] === 'number') {
        array = arguments[0];
        threads = arguments[1];
        target = new array.constructor(array.length);
        parameters = Array.prototype.slice.call(arguments, 2);
    } else if (typeof arguments[0] === 'object' && typeof arguments[0].length === 'number' &&
        typeof arguments[1] === 'object' && typeof arguments[1].length === 'number' &&
        typeof arguments[2] === 'number') {
        target = arguments[0];
        array = arguments[1];
        threads = arguments[2];
        parameters = Array.prototype.slice.call(arguments, 3);
    } else {
        return Promise.reject('invalid arguments');
    }
    if (!(threads > 0 && threads <= this.maxParallel)) return Promise.reject('threads must not exceed maxParallel');

    const chunkSize = Math.ceil(array.length / threads);

    const q = [];
    for (let start = 0, end = start + chunkSize;
        start < array.length;
        start = end, end = Math.min(start + chunkSize, array.length)) {

        q.push(this.mapAsync.apply(this, [target.subarray(start, end), array.subarray(start, end), ...parameters]));
    }

    return Promise.all(q).then(() => target);
}

for (const t of types) {
    if (addon[t] === undefined) {
        console.warn(`${t} type not built`);
        continue;
    }
    addon[t].prototype.mapMPAsync = mapMPAsync;
    for (const m of promisifiables) {
        addon[t].prototype[m] = (function () {
            const promisified = promisify(addon[t].prototype[m]);
            const original = addon[t].prototype[m];

            return function () {
                if (typeof arguments[arguments.length - 1] === 'function') {
                    return original.apply(this, arguments);
                }
                return promisified.apply(this, arguments);
            };
        })();
    }
}

module.exports = addon;
