import { Float64 } from 'exprtk.js';

import chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);
const assert = chai.assert;

const size = 10000;
const iterations = 250;

describe('stress', () => {
  let expr: Float64;
  let arrayInc: Float64Array, arrayDec: Float64Array;

  before(() => {
    expr = new Float64('a + b');
    arrayInc = new Float64Array(size);
    arrayDec = new Float64Array(size);
    for (let i = 0; i < size; i++) {
      arrayInc[i] = i;
      arrayDec[i] = size - i - 1;
    }
  });

  it('mapAsync', (done) => {
    let ready = 0;
    for (let i = 0; i < iterations; i++) {
      expr.mapAsync(expr.maxParallel, arrayInc, 'a', 4)
        .then((r) => {
          assert.equal(r.length, size);
          // This is the slowest and most expensive part
          // so we check 1 out of 100 elements
          for (let j = 0; j < r.length; j += r.length / 100)
            assert.closeTo(r[j], j + 4, 1e-9);
          if (++ready == iterations) done();
        })
        .catch((e) => done(e));
    }
  });

  it('cwiseAsync', (done) => {
    let ready = 0;
    for (let i = 0; i < iterations; i++) {
      expr.cwiseAsync(expr.maxParallel, {a: arrayInc, b: arrayDec})
        .then((r) => {
          assert.equal(r.length, size);
          for (let j = 0; j < r.length; j += r.length / 100)
            assert.closeTo(r[j], size - 1, 1e-9);
          if (++ready == iterations) done();
        })
        .catch((e) => done(e));
    }
  });
});
