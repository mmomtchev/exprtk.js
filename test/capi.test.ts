/* eslint-disable @typescript-eslint/no-explicit-any */
import * as path from 'path';
import binary from '@mapbox/node-pre-gyp';

import Expression from 'exprtk.js';

import chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);
const assert: Chai.AssertStatic = chai.assert;

declare module 'exprtk.js' {
  interface Expression {
    _CAPI_: ArrayBufferView;
  }
}
describe('Expression C-API', () => {
  let mean_f32: Expression.Float32, mean_u32: Expression.Uint32, vector: Expression.Uint32;
  let testAddon: any;

  afterEach((global as any).gc);
  
  before(() => {
    mean_u32 = new Expression.Uint32('(a + b) / 2');
    mean_f32 = new Expression.Float32('(a + b) / 2');
    vector = new Expression.Uint32('x[0] + x[1] + c', ['c'], { x: 2 });

    const binding_path = path.dirname(binary.find(path.resolve(path.join(__dirname, '../package.json'))));
    const test_addon_path = path.resolve(binding_path, 'exprtk.js-test.node');
    // eslint-disable-next-line @typescript-eslint/no-require-imports
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

    // multiple invocations should return the same object
    assert.strictEqual(desc_u32, mean_u32._CAPI_);
  });

  it('capi_eval()', () => {
    const r = testAddon.testEval(vector);
    assert.equal(r, 15);
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
    const expected = new Float64Array([5.5, 11, 16.5, 22, 27.5]);

    let r = testAddon.testCwise(mean_f32, false);
    assert.instanceOf(r, Float64Array);
    for (let i = 0; i < 5; i++) assert.closeTo(r[i], expected[i], 10e-9);

    r = testAddon.testCwise(mean_f32, true);
    assert.instanceOf(r, Float64Array);
    for (let i = 0; i < 5; i++) assert.closeTo(r[i], expected[i], 10e-9);
  });
});
