const b = require('benny');
const { assert } = require('chai');
const e = require('..');
const cpus = require('os').cpus().length;

// You should probably read the notes in `Performance.md`

module.exports = function (type, size, fn) {
  const fns = {
    'simple': {
      exprJS: (x) => (x * x + 2 * x + 1),
      exprExprTkI: new e[type]('x*x + 2*x + 1', ['x']),
      exprExprTkE: new e[type](
        'for (var i := 0; i < a1[]; i += 1) { var x := a1[i]; a2[i] := x*x + 2*x + 1; };',
        [], { a1: size, a2: size }),
    },
    'complex': {
      exprJS: (x) => (2 * Math.cos(x) / (Math.sqrt(x) + 1)),
      exprExprTkI: (type) => new e[type]('2 * cos(x) / (sqrt(x) + 1)', ['x']),
      exprExprTkE: new e[type](
        'for (var i := 0; i < a1[]; i += 1) { var x := a1[i]; a2[i] := 2 * cos(x) / (sqrt(x) + 1); };',
        [], { a1: size, a2: size }),
    }
  };

  const allocator = global[type + 'Array'];

  const { exprJS, exprExprTkE, exprExprTkI} = fns[fn];

  const a1 = new allocator(size);

  for (let i = 0; i < size; i++) a1[i] = i;

  // target array allocation is included
  return b.suite(
    `${fn} function, map() ${type} arrays of ${size} elements`,

    b.add('V8 / JS iterative for loop', () => {
      const r = new allocator(size);
      for (let i = 0; i < size; i++)
        r[i] = exprJS(a1[i]);
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    b.add('V8 / JS map()', () => {
      const r = a1.map(exprJS);
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    b.add('ExprTk.js map() traversal', () => {
      const r = exprExprTkI.map(a1, 'x');
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    // This test won't work with Int8/Int16 because of the iterator variable
    // which won't be able to hold the array index
    b.add('ExprTk.js eval() explicit traversal', () => {
      const r = new allocator(size);
      exprExprTkE.eval(a1, r);
      assert.equal(r[4], exprJS(4));
    }),
    b.add('ExprTk.js cwise() traversal', () => {
      const r = exprExprTkI.cwise({ x: a1 });
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    b.add(`ExprTk.js map() ${cpus}-way MP traversal`, () => {
      const r = exprExprTkI.map(cpus, a1, 'x');
      assert.instanceOf(r, allocator);
      assert.equal(r[4], exprJS(4));
    }),
    b.cycle(),
    b.complete()
  );
};
