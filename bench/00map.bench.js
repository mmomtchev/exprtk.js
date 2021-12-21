const b = require('benny');
const { assert } = require('chai');
const e = require('..');

module.exports = function (type, size) {
  const allocator = global[type + 'Array'];

  const a1 = new allocator(size);

  for (let i = 0; i < size; i++) a1[i] = i;

  const exprJS = (x) => (x * x + 2 * x + 1);
  const exprExprTkI = new e[type]('x*x + 2*x + 1', ['x']);
  const exprExprTkE = new e[type](
    'for (var i := 0; i < a1[]; i += 1) { var x := a1[i]; a2[i] := x*x + 2*x + 1; };',
    [], { a1: size, a2: size });

  // To be fair to exprJS we should not use it outside of the bench
  // V8 might label it megamorphic
  const testFn = (x) => (x * x + 2 * x + 1);

  return b.suite(
    `map() ${type} arrays of ${size} elements`,

    b.add('V8 / JS', () => {
      const r = a1.map(exprJS);
      assert.instanceOf(r, allocator);
      assert.equal(r[4], testFn(4));
    }),
    b.add('ExprTk.js map() traversal', () => {
      const r = exprExprTkI.map(a1, 'x');
      assert.instanceOf(r, allocator);
      assert.equal(r[4], testFn(4));
    }),
    // This test won't work with Int8/Int16 because of the iterator variable
    // which won't be able to hold the array index
    b.add('ExprTk.js eval() explicit traversal', () => {
      const a2 = new allocator(size);
      exprExprTkE.eval(a1, a2);
      assert.equal(a2[4], testFn(4));
    }),
    b.add('ExprTk.js cwise() traversal', () => {
      const r = exprExprTkI.cwise({ x: a1 });
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    b.cycle(),
    b.complete()
  );
};
