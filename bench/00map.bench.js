const b = require('benny');
const { assert } = require('chai');
const e = require('..');

module.exports = function (type, size) {
  const allocator = global[type + 'Array'];

  const a1 = new allocator(size);

  const exprJS = (x) => (x * x + 2 * x + 1);
  const exprExprTkI = new e[type]('x*x + 2*x + 1', ['x']);

  return b.suite(
    `map() ${type} arrays of ${size} elements`,

    b.add('V8 / JS', () => {
      const r = a1.map(exprJS);
      assert.instanceOf(r, allocator);
    }),
    b.add('ExprTk.js (implicit traversal)', () => {
      const r = exprExprTkI.map(a1, 'x');
      assert.instanceOf(r, allocator);
    }),
    b.cycle(),
    b.complete()
  );
}