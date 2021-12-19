const Expression = require("../lib/binding.js");

const chai = require('chai');
const chaiAsPromised = require('chai-as-promised');
chai.use(chaiAsPromised)
const assert = chai.assert;
const expr = Expression.Float64;

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

    describe('types', () => {
        it('Float32', () => {
            const id32 = new Expression.Float32('(a + b) / 2', ['a', 'b']);
            const a32 = new Float32Array([ 1, 2, 3, 4]);
            const r32 = id32.map(a32, 'a', 1);
            assert.instanceOf(r32, Float32Array);
            assert.deepEqual(r32, new Float32Array([1, 1.5, 2, 2.5]));
        })

        it('Float64', () => {
            const id64 = new Expression.Float64('(a + b) / 2', ['a', 'b']);
            const a64 = new Float64Array([1, 2, 3, 4]);
            const r64 = id64.map(a64, 'a', 1);
            assert.instanceOf(r64, Float64Array);
            assert.deepEqual(r64, new Float64Array([1, 1.5, 2, 2.5]));
        })
    })

    describe('compute', () => {
        let mean, vectorMean, pi, clamp, sumPow;
        let vector = new Float64Array([1, 2, 3, 4, 5, 6]);
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
            vectorMean = new expr(
                'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                [], { 'x': 6 });
            pi = new expr('22 / 7', []);
            clamp = new expr('clamp(minv, x, maxv)', ['minv', 'x', 'maxv']);
            sumPow = new expr('a + pow(x, p)', ['a', 'x', 'p']);
        })

        describe('eval() object form', () => {
            it('should accept variables', () => {
                const r = mean.eval({ a: 5, b: 10 });
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            })
            it('should accept vectors', () => {
                const r = vectorMean.eval({ x: vector });
                assert.closeTo(r, 3.5, 10e-9);
            })
            it('should throw if not all arguments are given', () => {
                assert.throws(() => {
                    const r = vectorMean.eval();
                }, /wrong number of input arguments/)
            })
            it('should support expression without arguments', () => {
                assert.closeTo(pi.eval(), 3.14, 0.01);
            })
        })

        describe('evalAsync() object form', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.evalAsync({ a: 5, b: 10 }), (5 + 10) / 2, 10e-9);
            })
            it('should accept vectors', () => {
                return assert.eventually.closeTo(vectorMean.evalAsync({ x: vector }), 3.5, 10e-9);
            })
            it('should reject if not all arguments are given', () => {
                return assert.isRejected(vectorMean.evalAsync({}), /wrong number of input arguments/);
            })
            it('should support expression without arguments', () => {
                return assert.eventually.closeTo(pi.evalAsync({}), 3.14, 0.01);
            })
        })

        describe('eval() argument list form', () => {
            it('should accept variables', () => {
                const r = mean.eval(5, 10);
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            })
            it('should accept a TypedArray', () => {
                assert.instanceOf(mean, expr);
                const r = vectorMean.eval(vector);
                assert.closeTo(r, 3.5, 10e-9);
            })
            it('should throw w/ vector that is not a Float64Array', () => {
                assert.throws(() => {
                    vectorMean.eval(new Uint32Array(6))
                }, /vector data must be a Float64Array/)
            })
            it('should throw w/ vector that is not the right size', () => {
                assert.throws(() => {
                    vectorMean.eval(new Float64Array(4))
                }, /size 4 does not match declared size 6/)
            })
        })

        describe('evalAsync() argument list form', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.evalAsync(5, 10), (5 + 10) / 2, 10e-9);
            })
            it('should accept an TypedArray', () => {
                return assert.eventually.closeTo(vectorMean.evalAsync(vector), 3.5, 10e-9);
            })
            it('should reject w/ vector that is not a Float64Array', () => {
                return assert.isRejected(vectorMean.evalAsync(new Uint32Array(6)), /vector data must be a Float64Array/);
            })
            it('should reject w/ vector that is not the right size', () => {
                return assert.isRejected(vectorMean.evalAsync(new Float64Array(4)), /size 4 does not match declared size 6/);
            })
            it('should work with concurrent invocations', () => {
                const big = 128 * 1024;
                const vectorMean = new expr(
                    'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                    [], { 'x': big });
                const a1 = new Float64Array(big);
                a1.fill(12);
                const a2 = new Float64Array(big);
                a2.fill(15);
                return assert.isFulfilled(Promise.all([
                    vectorMean.evalAsync(a1), vectorMean.evalAsync(a2)
                ]).then(([m1, m2]) => {
                    assert.closeTo(m1, 12, 10e-9);
                    assert.closeTo(m2, 15, 10e-9);
                }))
            })
        })

        describe('map()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = clamp.map(vector, 'x', 2, 4);
                assert.instanceOf(r, Float64Array);
                assert.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            })

            it('should throw w/o iterator', () => {
                assert.throws(() => {
                    clamp.map(vector, 2, 4);
                }, /second argument must be the iterator variable name/)
            })

            it('should throw w/ invalid iterator', () => {
                assert.throws(() => {
                    clamp.map(vector, 'c', 2, 4);
                }, /c is not a declared scalar variable/)
            })

            it('should throw w/ invalid variables', () => {
                assert.throws(() => {
                    clamp.map(vector, 'x', 4);
                }, /wrong number of input arguments/)
            })
        })

        describe('mapAsync()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = clamp.mapAsync(vector, 'x', 2, 4);
                return assert.eventually.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            })

            it('should reject w/o iterator', () => {
                return assert.isRejected(clamp.mapAsync(vector, 2, 4), /second argument must be the iterator variable name/)
            })

            it('should reject w/ invalid iterator', () => {
                return assert.isRejected(clamp.mapAsync(vector, 'c', 2, 4), /c is not a declared scalar variable/)
            })

            it('should reject w/ invalid variables', () => {
                return assert.isRejected(clamp.mapAsync(vector, 'x', 4), /wrong number of input arguments/)
            })
        })

        describe('reduce()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = sumPow.reduce(vector, 'x', 'a', 0, 2);
                assert.isNumber(r);
                assert.equal(r, 91);
            })

            it('should throw w/o iterator', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 2, 4);
                }, /second argument must be the iterator variable name/);
            })

            it('should throw w/ invalid iterator', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'c', 'a', 4);
                }, /c is not a declared scalar variable/);
            })

            it('should throw w/o accumulator', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x');
                }, /third argument must be the accumulator variable name/);
            })

            it('should throw w/ invalid accumulator', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'b', 4);
                }, /b is not a declared scalar variable/);
            })

            it('should throw w/o accumulator initial value', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'a');
                }, /fourth argument must be a number for the accumulator initial value/);
            })

            it('should throw w/ invalid accumulator initial value', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'a', 'd');
                }, /fourth argument must be a number for the accumulator initial value/);
            })

            it('should throw w/ invalid variables', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'a', 0);
                }, /wrong number of input arguments/);
            })
        })

        describe('reduceAsync()', () => {

            it('should evaluate an expression over all values of an array', () => {
                r = sumPow.reduceAsync(vector, 'x', 'a', 0, 2);
                return assert.eventually.equal(r, 91);
            })

            it('should throw w/o iterator', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 2, 4), /second argument must be the iterator variable name/);
            })

            it('should throw w/ invalid iterator', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'c', 'a', 4), /c is not a declared scalar variable/);
            })

            it('should throw w/o accumulator', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x'), /third argument must be the accumulator variable name/);
            })

            it('should throw w/ invalid accumulator', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'b', 4), /b is not a declared scalar variable/);
            })

            it('should throw w/o accumulator initial value', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'a'), /fourth argument must be a number for the accumulator initial value/);
            })

            it('should throw w/ invalid accumulator initial value', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'a', 'd'), /fourth argument must be a number for the accumulator initial value/);
            })

            it('should throw w/ invalid variables', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'a', 0), /wrong number of input arguments/);
            })
        })
    })
})
