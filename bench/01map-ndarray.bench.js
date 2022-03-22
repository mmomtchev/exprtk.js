const b = require('benny');
const { assert } = require('chai');
const e = require('..');
const cpus = require('os').cpus().length;
const ndarray = require('ndarray');
const ndarray_cwise = require('cwise');

// You should probably read the notes in `Performance.md`

const id = ndarray_cwise({
  args: ['shape', 'index', 'array'],
  body: function (s, i, a) {
    // eslint-disable-next-line no-unused-vars
    a = i[0] * s[1] + i[1];
  }
});

module.exports = function (type, size) {
  const allocator = global[type + 'Array'];

  const size1d = Math.round(Math.sqrt(size));

  const input = ndarray(new allocator(size1d * size1d), [size1d, size1d]);
  id(input);

  const exprJS = (x) => (x * x + 2 * x + 1);
  const exprExprTk = new e[type]('x*x + 2*x + 1', ['x']);

  const exprCwise = ndarray_cwise({
    args: ['array', 'array'],
    body: function (r, x) { r = (x * x + 2 * x + 1); }
  });

  const expected = ndarray(new allocator(size1d * size1d), [size1d, size1d]);
  for (const i in input.data)
    expected.data[i] = exprJS(input.data[i]);

  return b.suite(
    `cwise() ${type} ndarrays of ${size1d}x${size1d} elements`,

    b.add('ndarray cwise()', () => {
      const r = ndarray(new allocator(size1d * size1d), [size1d, size1d]);
      exprCwise(r, input);
      assert.instanceOf(r.data, allocator);
      assert.equal(r.data[4], expected.data[4]);
    }),
    b.add('ExprTk.js cwise() traversal', () => {
      const r = exprExprTk.cwise({ x: input });
      assert.instanceOf(r, allocator);
      assert.equal(r[4], expected.data[4]);
    }),
    b.add(`ExprTk.js cwise() ${cpus}-way MP traversal`, () => {
      const r = exprExprTk.cwise(cpus, { x: input });
      assert.instanceOf(r, allocator);
      assert.equal(r[4], expected.data[4]);
    }),
    b.cycle(),
    b.complete()
  );
};
