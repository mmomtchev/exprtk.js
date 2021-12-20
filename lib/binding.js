const path = require('path');
const binary = require('@mapbox/node-pre-gyp');
const binding_path = binary.find(path.resolve(path.join(__dirname, '../package.json')));
const addon = require(binding_path);

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
