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

module.exports = function (type, size, fn) {
  const allocator = global[type + 'Array'];

  const size1d = Math.round(Math.sqrt(size));

  const input = ndarray(new allocator(size1d * size1d), [size1d, size1d]);
  id(input);

  const fns = {
    'simple': {
      exprJS: (x) => (x * x + 2 * x + 1),
      exprExprTk: new e[type]('x*x + 2*x + 1', ['x']),
      exprCwise: ndarray_cwise({
        args: ['array', 'array'],
        body: function (r, x) { r = (x * x + 2 * x + 1); }
      })
    },
    'complex': {
      exprJS: (x) => (2 * Math.cos(x) / (Math.sqrt(x) + 1)),
      exprExprTk: new e[type]('2 * cos(x) / (sqrt(x) + 1)', ['x']),
      exprCwise: ndarray_cwise({
        args: ['array', 'array'],
        body: function (r, x) { r = 2 * Math.cos(x) / (Math.sqrt(x) + 1); }
      })
    }
  };

  const { exprJS, exprCwise, exprExprTk } = fns[fn];

  const expected = ndarray(new allocator(size1d * size1d), [size1d, size1d]);
  for (const i in input.data)
    expected.data[i] = exprJS(input.data[i]);

  const r = ndarray(new allocator(size1d * size1d), [size1d, size1d]);

  // target array allocation is not included
  return b.suite(
    `${fn} function, cwise() ${type} ndarrays of ${size1d}x${size1d} elements`,

    b.add('ndarray cwise()', () => {
      exprCwise(r, input);
      assert.instanceOf(r.data, allocator);
      assert.equal(r.data[4], expected.data[4]);
    }),
    b.add('ExprTk.js cwise() traversal', () => {
      exprExprTk.cwise({ x: input }, r.data);
      assert.equal(r.data[4], expected.data[4]);
    }),
    b.add(`ExprTk.js cwise() ${cpus}-way MP traversal`, () => {
      exprExprTk.cwise(cpus, { x: input }, r.data);
      assert.equal(r.data[4], expected.data[4]);
    }),
    b.cycle(),
    b.complete()
  );
};
