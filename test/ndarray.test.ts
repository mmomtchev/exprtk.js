import { Float64 as Float64Expression } from 'exprtk.js';
import ndarray from 'ndarray';
import ops from 'ndarray-ops';
import array from '@stdlib/ndarray/array';

import chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);
const assert: Chai.AssertStatic = chai.assert;

describe('ndarray support', () => {
    let expr: Float64Expression;
    const rowMajor = ndarray(new Float64Array(6), [2, 3], [3, 1]);
    const colMajor = ndarray(new Float64Array(6), [2, 3], [1, 2]);
    const rowNegative = ndarray(new Float64Array(6), [2, 3], [-3, -1]);
    const colNegative = ndarray(new Float64Array(6), [2, 3], [-1, -2]);
    const stdlibArrayRow = array(new Float64Array(6), { shape: [2, 3], order: 'row-major' });
    const stdlibArrayCol = array(new Float64Array(6), { shape: [2, 3], order: 'column-major' });
    const expected = ndarray(new Float64Array(6), [2, 3], [3, 1]);

    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    afterEach((global as any).gc);

    before(() => {
        expr = new Float64Expression('a + b');
        for (let y = 0; y < 2; y++)
            for (let x = 0; x < 3; x++) {
                rowMajor.set(y, x, y * 3 + x);
                colMajor.set(y, x, y * 3 + x);
                rowNegative.set(y, x, y * 3 + x);
                colNegative.set(y, x, y * 3 + x);
                stdlibArrayRow.set(y, x, y * 3 + x);
                stdlibArrayCol.set(y, x, y * 3 + x);
                expected.set(y, x, (y * 3 + x) * 2);
            }
    });

    describe('cwise()', () => {

        it('should accept scijs/ndarray', () => {
            const result = ndarray(new Float64Array(6), [2, 3]);

            const r = expr.cwise({ a: rowMajor, b: colMajor }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected));
        });

        it('should accept @stdlib/ndarray', () => {
            const result = ndarray(new Float64Array(6), [2, 3]);

            const r = expr.cwise({ a: stdlibArrayRow, b: stdlibArrayCol }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected));
        });

        it('should accept negative stride ndarrays', () => {
            const result = ndarray(new Float64Array(6), [2, 3]);

            const r = expr.cwise({ a: rowNegative, b: colNegative }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected));
        });

        it('should accept ndarray hi views', () => {
            const result = ndarray(new Float64Array(4), [2, 2]);

            const r = expr.cwise({ a: rowMajor.hi(2, 2), b: colMajor.hi(2, 2) }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected.hi(2, 2)));
        });

        it('should accept ndarray lo views', () => {
            const result = ndarray(new Float64Array(4), [2, 2]);

            const r = expr.cwise({ a: rowMajor.lo(0, 1), b: colMajor.lo(0, 1) }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected.lo(0, 1)));
        });

        it('should support MP joblets', () => {
            const result = ndarray(new Float64Array(6), [2, 3]);

            const r = expr.cwise(2, { a: rowMajor, b: colMajor }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result, expected));
        });

        it('should support odd MP joblets', function () {
            // This test requires at least 3 CPU cores
            if (Float64Expression.maxParallel < 3)
                this.skip();
            const result = ndarray(new Float64Array(6), [2, 2]);

            const r = expr.cwise(3, { a: rowMajor.hi(2, 2), b: colMajor.hi(2, 2) }, result.data);
            assert.strictEqual(result.data, r);
            assert.isTrue(ops.equals(result.hi(2, 2), expected.hi(2, 2)));
        });

        it('should throw with invalid ndarrays', () => {
            assert.throws(() => {
                expr.cwise({
                    a: { data: new Float64Array(6), shape: [2, 3], stride: [5, 5] } as ndarray.NdArray<Float64Array>,
                    b: colMajor
                }, new Float64Array(6));
            }, /invalid strided array, ArrayBuffer overflow/);
            assert.throws(() => {
                expr.cwise({
                    a: { 
                        data: new Float64Array(6),
                        shape: [2, 3],
                        offset: 4,
                        stride: [-3, -1]
                    } as ndarray.NdArray<Float64Array>,
                    b: colMajor
                }, new Float64Array(6));
            }, /invalid strided array, ArrayBuffer overflow/);
            assert.throws(() => {
                expr.cwise({
                    a: { data: new Float64Array(5), shape: [2, 3], stride: [3, 1] } as ndarray.NdArray<Float64Array>,
                    b: colMajor
                }, new Float64Array(6));
            }, /invalid strided array, ArrayBuffer overflow/);
        });

        it('should throw with non-matching ndarrays', () => {
            assert.throws(() => {
                expr.cwise({
                    a: ndarray(new Float64Array(6), [1, 2, 3]),
                    b: colMajor
                }, new Float64Array(6));
            }, /all strided arrays must have the same number of dimensions/);
            assert.throws(() => {
                expr.cwise({
                    a: ndarray(new Float64Array(6), [3, 2]),
                    b: colMajor
                }, new Float64Array(6));
            }, /all strided arrays must have the same shape/);
        });
    });
});
