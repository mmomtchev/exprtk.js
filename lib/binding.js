const addon = require('../build/Debug/exprtk.js-native');
const promisify = require('util').promisify;

const promisifiables = [
    'evalAsync',
    'fnAsync'
];

for (const m of promisifiables) {
    addon.Expression.prototype[m] = (function () {
        const promisified = promisify(addon.Expression.prototype[m]);
        const original = addon.Expression.prototype[m];

        return function() {
            if (typeof arguments[arguments.length - 1] === 'function') {
                return original.apply(this, arguments);
            }
            return promisified.apply(this, arguments);
        }
    })();
}

module.exports = addon;
