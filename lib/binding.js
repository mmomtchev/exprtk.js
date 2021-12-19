const addon = require('../build/Debug/exprtk.js-native');
const promisify = require('util').promisify;

const types = [
    'Float32',
    'Float64'
];

const promisifiables = [
    'evalAsync',
    'mapAsync',
    'reduceAsync'
];

for (const t of types) {
    for (const m of promisifiables) {
        addon[t].prototype[m] = (function () {
            const promisified = promisify(addon[t].prototype[m]);
            const original = addon[t].prototype[m];

            return function () {
                if (typeof arguments[arguments.length - 1] === 'function') {
                    return original.apply(this, arguments);
                }
                return promisified.apply(this, arguments);
            }
        })();
    }
}

module.exports = addon;
