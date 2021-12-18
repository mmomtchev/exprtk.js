const expr = require("../lib/binding.js").Expression;

const chai = require('chai');
const chaiAsPromised = require('chai-as-promised');
chai.use(chaiAsPromised)
const assert = chai.assert;

describe('Expression', () => {
    describe('constructor', () => {
        it('should throw w/o arguments', () => {
            assert.throws(() => {
                new expr();
            }, /mandatory/)
        })
        it('should throw w/ invalid arguments 1', () => {
            assert.throws(() => {
                new expr(12);
            }, /mandatory/)
        })
        it('should throw w/ invalid arguments 2', () => {
            assert.throws(() => {
                new expr('a', 12);
            }, /must be an array/)
        })
        it('should throw w/ invalid expression', () => {
            assert.throws(() => {
                new expr('a ! b', ['a', 'b']);
            }, /failed compiling/)
        })
        it('should throw w/ invalid vector', () => {
            assert.throws(() => {
                new expr('a', ['a'], 12);
            }, /vectors must be an object/)
        })
        it('should throw w/ vector that is not a number', () => {
            assert.throws(() => {
                new expr('a', ['a'], { x: '12' });
            }, /vector size must be a number/)
        })
        it('should accept an expression w/ variables', () => {
            const mean = new expr('(a + b) / 2', ['a', 'b']);
            assert.instanceOf(mean, expr);
        })
        it('should accept an expression w/ vectors', () => {
            const mean = new expr('a', ['a'], { x: 12 });
            assert.instanceOf(mean, expr);
        })
    })

    describe('compute', () => {
        let mean, vectorMean;
        let vector = new Float64Array([1, 2, 3, 4, 5, 6]);
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
            vectorMean = new expr(
                'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                [], { 'x': 6 });
        })

        describe('eval()', () => {
            it('should accept variables', () => {
                const r = mean.eval({ a: 5, b: 10 });
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            })
            it('should accept vectors', () => {
                const r = vectorMean.eval({ x: vector });
                assert.closeTo(r, 3.5, 10e-9);
            })
        })

        describe('evalAsync()', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.evalAsync({ a: 5, b: 10 }), (5 + 10) / 2, 10e-9);
            })
        })

        describe('fn()', () => {
            it('should accept variables', () => {
                const r = mean.fn(5, 10);
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            })
            it('should accept an TypedArray', () => {
                assert.instanceOf(mean, expr);
                const r = vectorMean.fn(vector);
                assert.closeTo(r, 3.5, 10e-9);
            })
            it('should throw w/ vector that is not a Float64Array', () => {
                assert.throws(() => {
                    vectorMean.fn(new Uint32Array(6))
                }, /vector data must be a Float64Array/)
            })
            it('should throw w/ vector that is not the right size', () => {
                assert.throws(() => {
                    vectorMean.fn(new Float64Array(4))
                }, /size 4 does not match declared size 6/)
            })
        })

        describe('fnAsync()', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.fnAsync(5, 10), (5 + 10) / 2, 10e-9);
            })
            it('should accept an TypedArray', () => {
                return assert.eventually.closeTo(vectorMean.fnAsync(vector), 3.5, 10e-9);
            })
            it('should reject w/ vector that is not a Float64Array', () => {
                return assert.isRejected(vectorMean.fnAsync(new Uint32Array(6)), /vector data must be a Float64Array/);
            })
            it('should reject w/ vector that is not the right size', () => {
                return assert.isRejected(vectorMean.fnAsync(new Float64Array(4)), /size 4 does not match declared size 6/);
            })
        })
    })
})
