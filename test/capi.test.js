const path = require('path');
const binary = require('@mapbox/node-pre-gyp');

const Expression = require('../lib/binding.js');

const chai = require('chai');
const chaiAsPromised = require('chai-as-promised');
chai.use(chaiAsPromised);
const assert = chai.assert;

describe('Expression C-API', () => {
  let mean_f32, mean_u32, vector;
  let testAddon;

  before(() => {
    mean_u32 = new Expression.Uint32('(a + b) / 2');
    mean_f32 = new Expression.Float32('(a + b) / 2');
    vector = new Expression.Int16('x[0] + x[1] + c', ['c'], { x: 2 });

    const binding_path = path.dirname(binary.find(path.resolve(path.join(__dirname, '../package.json'))));
    const test_addon_path = path.resolve(binding_path, 'exprtk.js-test.node');
    testAddon = require(test_addon_path);
  });

  it('should load the test addon', () => {
    assert.isDefined(testAddon);
    assert.isFunction(testAddon.testEval);
  });

  it('should return a C-API descriptor', () => {
    const desc_u32 = mean_u32._CAPI_;
    assert.instanceOf(desc_u32, ArrayBuffer);
    assert.equal(desc_u32.byteLength, 112);

    const desc_f32 = mean_f32._CAPI_;
    assert.instanceOf(desc_f32, ArrayBuffer);
    assert.equal(desc_f32.byteLength, 112);

    const desc_vector = vector._CAPI_;
    assert.instanceOf(desc_vector, ArrayBuffer);
    assert.equal(desc_vector.byteLength, 120);
  });

  it('capi_eval()', () => {
    const r = testAddon.testEval(mean_u32);
    assert.equal(r, 14);
  });

  it('capi_map()', () => {
    const r = testAddon.testMap(mean_u32);
    assert.instanceOf(r, Uint32Array);
    assert.deepEqual(r, new Uint32Array([15, 20, 25, 30, 35, 40]));
  });

  it('capi_reduce()', () => {
    const r = testAddon.testReduce(mean_u32);
    assert.equal(r, 50);
  });

  it('capi_cwise()', () => {
    const r = testAddon.testCwise(mean_f32);
    assert.instanceOf(r, Float64Array);
    const expected = new Float64Array([5.5, 11, 16.5, 22, 27.5]);
    for (let i = 0; i < 5; i++) assert.closeTo(r[i], expected[i], 10e-9);
  });
});
