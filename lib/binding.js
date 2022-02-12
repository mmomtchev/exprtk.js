const path = require('path');
const binary = require('@mapbox/node-pre-gyp');
const binding_path = binary.find(path.resolve(path.join(__dirname, '../package.json')));
const addon = require(binding_path);

const promisify = require('util').promisify;

addon.Expression = class Expression {};

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

for (const t of types) {
    if (addon[t] === undefined) {
        console.warn(`${t} type not built`);
        continue;
    }
    Object.setPrototypeOf(addon[t].prototype, addon.Expression.prototype);
    Object.setPrototypeOf(addon[t], addon.Expression);
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
