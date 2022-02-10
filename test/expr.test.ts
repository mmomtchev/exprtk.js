/* eslint-disable @typescript-eslint/no-explicit-any */
import Expression, { Float64 } from '..';
import { Float64 as expr } from '..';

import { exec } from 'child_process';
import * as asyncHooks from 'async_hooks';
import { inspect } from 'util';
import * as os from 'os';

import chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);
const assert = chai.assert;

describe('Expression', () => {
    afterEach((global as any).gc);

    describe('constructor', () => {
        it('should throw w/o arguments', () => {
            assert.throws(() => {
                new (expr as any)();
            }, /mandatory/);
        });
        it('should throw w/ invalid arguments 1', () => {
            assert.throws(() => {
                new (expr as any)(12);
            }, /expresion must be a string/);
        });
        it('should throw w/ invalid arguments 2', () => {
            assert.throws(() => {
                new (expr as any)('a', 12);
            }, /must be an array/);
        });
        it('should throw w/ invalid expression', () => {
            assert.throws(() => {
                new expr('a ! b', ['a', 'b']);
            }, /failed compiling/);
        });
        it('should throw w/ invalid variable name', () => {
            assert.throws(() => {
                new expr('1a ! b', ['1a', 'b']);
            }, /not a valid variable name/);
        });
        it('should throw w/ invalid vector', () => {
            assert.throws(() => {
                new (expr as any)('a', ['a'], 12);
            }, /vectors must be an object/);
        });
        it('should throw w/ vector that is not a number', () => {
            assert.throws(() => {
                new (expr as any)('a', ['a'], { x: '12' });
            }, /vector size must be a number/);
        });
        it('should throw w/ invalid vector name', () => {
            assert.throws(() => {
                new (expr as any)('a', ['a'], { '1x': 5 });
            }, /not a valid vector name/);
        });
        it('should accept an expression w/o explicit arguments', () => {
            const mean = new expr('(a + b) / 2');
            assert.instanceOf(mean, expr);
            assert.equal(mean.expression, '(a + b) / 2');
            assert.equal(mean.toString(), '(a + b) / 2');
            assert.equal(inspect(mean), 'Float64Expression [(a + b) / 2] {}');
            assert.deepEqual(mean.scalars, ['a', 'b']);
            assert.deepEqual(mean.vectors, {});

            assert.equal(mean.type, 'Float64');
        });
        it('should accept an expression w/ scalars', () => {
            const mean = new expr('(a + b) / 2', ['a', 'b']);
            assert.instanceOf(mean, expr);
            assert.equal(mean.expression, '(a + b) / 2');
            assert.deepEqual(mean.scalars, ['a', 'b']);
            assert.deepEqual(mean.vectors, {});
            assert.equal(mean.type, 'Float64');
        });
        it('should accept an expression w/ vectors', () => {
            const mean = new expr('a + x[10]', ['a'], { x: 12 });
            assert.instanceOf(mean, expr);
            assert.equal(mean.expression, 'a + x[10]');
            assert.deepEqual(mean.scalars, ['a']);
            assert.deepEqual(mean.vectors, { x: 12 });
            assert.equal(mean.type, 'Float64');
            assert.equal(mean.maxParallel, os.cpus().length);
        });
        it('should support setting the max parallel instances', () => {
            const mean = new Float64('a + x[10]', ['a'], { x: 12 });
            assert.equal(mean.maxParallel, os.cpus().length);
            mean.maxParallel = 1;
            assert.equal(mean.maxParallel, 1);
            assert.throws(() => {
                mean.maxParallel = 10e3;
            }, /environment variable EXPRTKJS_THREADS/);
        });
        it(`should have \`os.cpus().length=${os.cpus().length}\` number of worker threads by default`, () => {
            assert.equal(Float64.maxParallel, os.cpus().length);
        });
        it('should support setting the number of worker threads', (done) => {
            const testCode = '"const expr = require(\'.\').Float64; console.log(expr.maxParallel);"';
            let execPath = process.execPath;
            if (process.platform === 'win32') {
                // quotes to avoid errors like ''C:\Program' is not recognized as an internal or external command'
                execPath = `"${execPath}"`;
            }
            exec(
                `${execPath} ${['-e', testCode].join(' ')}`,
                { env: { EXPRTKJS_THREADS: '11' } },
                (err, stdout) => {
                    if (err) done(err);
                    assert.equal(stdout.trim(), '11');
                    done();
                });
        });
    });

    describe('types', () => {
        it('Int8', () => {
            const idi8 = new Expression.Int8('(a + b) / 2', ['a', 'b']);
            const ai8 = new Int8Array([1, 2, 3, 4]);
            const ri8 = idi8.map(ai8, 'a', 1);
            assert.instanceOf(ri8, Int8Array);
            assert.deepEqual(ri8, new Int8Array([1, 1, 2, 2]));
            assert.instanceOf(idi8.mapAsync(ai8, 'a', 1), Promise);
        });

        it('Uint8', () => {
            const idui8 = new Expression.Uint8('(a + b) / 2', ['a', 'b']);
            const aui8 = new Uint8Array([1, 2, 3, 4]);
            const rui8 = idui8.map(aui8, 'a', 1);
            assert.instanceOf(rui8, Uint8Array);
            assert.deepEqual(rui8, new Uint8Array([1, 1, 2, 2]));
            assert.instanceOf(idui8.mapAsync(rui8, 'a', 1), Promise);
        });

        it('Int16', () => {
            const idi16 = new Expression.Int16('(a + b) / 2', ['a', 'b']);
            const ai16 = new Int16Array([1, 2, 3, 4]);
            const ri16 = idi16.map(ai16, 'a', 1);
            assert.instanceOf(ri16, Int16Array);
            assert.deepEqual(ri16, new Int16Array([1, 1, 2, 2]));
            assert.instanceOf(idi16.mapAsync(ai16, 'a', 1), Promise);
        });

        it('Uint16', () => {
            const idui16 = new Expression.Uint16('(a + b) / 2', ['a', 'b']);
            const aui16 = new Uint16Array([1, 2, 3, 4]);
            const rui16 = idui16.map(aui16, 'a', 1);
            assert.instanceOf(rui16, Uint16Array);
            assert.deepEqual(rui16, new Uint16Array([1, 1, 2, 2]));
            assert.instanceOf(idui16.mapAsync(rui16, 'a', 1), Promise);
        });

        it('Int32', () => {
            const idi32 = new Expression.Int32('(a + b) / 2', ['a', 'b']);
            const ai32 = new Int32Array([1, 2, 3, 4]);
            const ri32 = idi32.map(ai32, 'a', 1);
            assert.instanceOf(ri32, Int32Array);
            assert.deepEqual(ri32, new Int32Array([1, 1, 2, 2]));
            assert.instanceOf(idi32.mapAsync(ai32, 'a', 1), Promise);
        });

        it('Uint32', () => {
            const idui32 = new Expression.Uint32('(a + b) / 2', ['a', 'b']);
            const aui32 = new Uint32Array([1, 2, 3, 4]);
            const rui32 = idui32.map(aui32, 'a', 1);
            assert.instanceOf(rui32, Uint32Array);
            assert.deepEqual(rui32, new Uint32Array([1, 1, 2, 2]));
            assert.instanceOf(idui32.mapAsync(rui32, 'a', 1), Promise);
        });

        it('Float32', () => {
            const id32 = new Expression.Float32('(a + b) / 2', ['a', 'b']);
            const a32 = new Float32Array([1, 2, 3, 4]);
            const r32 = id32.map(a32, 'a', 1);
            assert.instanceOf(r32, Float32Array);
            assert.deepEqual(r32, new Float32Array([1, 1.5, 2, 2.5]));
            assert.instanceOf(id32.mapAsync(r32, 'a', 1), Promise);
        });

        it('Float64', () => {
            const id64 = new Expression.Float64('(a + b) / 2', ['a', 'b']);
            const a64 = new Float64Array([1, 2, 3, 4]);
            const r64 = id64.map(a64, 'a', 1);
            assert.instanceOf(r64, Float64Array);
            assert.deepEqual(r64, new Float64Array([1, 1.5, 2, 2.5]));
            assert.instanceOf(id64.mapAsync(r64, 'a', 1), Promise);
        });
    });

    describe('compute', () => {
        let mean: Expression.Float64;
        let vectorMean: Expression.Float64;
        let pi: Expression.Float64;
        let clamp: Expression.Float64;
        let sumPow: Expression.Float64;
        let vectorFill: Expression.Float64;
        const vector = new Float64Array([1, 2, 3, 4, 5, 6]);
        const big = 128 * 1024;
        let bigarray: Float64Array, plus: Float64;

        let density: Expression.Float64;
        const R = 0.0831446;
        const Md = 0.0289652;
        const Mv = 0.018016;
        const phi = new Float32Array([0, 0.2, 0.5, 0.9, 0.5]);
        const P = new Uint16Array([1013, 1013, 1013, 1013, 995]);
        const T = new Uint16Array([25, 25, 25, 25, 25]);

        const expected = [
            1.1836,
            1.1808,
            1.1766,
            1.1710,
            1.1556,
        ];

        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
            vectorMean = new expr(
                'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                [], { 'x': 6 });
            pi = new expr('22 / 7', []);
            clamp = new expr('clamp(minv, x, maxv)', ['minv', 'x', 'maxv']);
            sumPow = new expr('a + pow(x, p)', ['a', 'x', 'p']);
            vectorFill = new expr(
                'for (var i := 0; i < 10; i += 1) { x[i] := i; }; x[];',
                [], { 'x': 10 });

            bigarray = new Float64Array(big);
            for (let i = 0; i < big; i++) bigarray[i] = i;
            plus = new expr('a + b');

            // Air density of humid air from relative humidity (phi), temperature (T) and pressure (P)
            // rho = ( Pd * Md + Pv * Mv ) / ( R * T )     // density (Avogadro's law)
            // Pv = phi * Ps                               // vapor pressure of water
            // Ps = 6.1078 * 10 ^ (7.5 * T / (T + 237.3))  // saturation vapor pressure (Tetens' equation)
            // Pd = P - Pv                                 // partial pressure of dry air
            // R = 0.0831446                               // universal gas constant
            // Md = 0.0289652                              // molar mass of water vapor
            // Mv = 0.018016                               // molar mass of dry air
            // ( this is the weather science form of the equation
            //   and not the hard physics one with T in C° and pressure in hPa )
            // phi, T and P are arbitrary TypedArrays of the same size
            //
            // Calculation uses Float64 internally
            // Result is stored in Float32
            //
            density = new expr(
                'var Pv := ( phi * 6.1078 * pow(10, (7.5 * T / (T + 237.3))) ); ' +  // compute Pv and store it
                '( (P - Pv) * Md + Pv * Mv ) / ( R * (T + 273.15) )',               // main formula (return expression)
                ['P', 'T', 'phi', 'R', 'Md', 'Mv']
            );

        });

        describe('eval() object form', () => {
            it('should accept variables', () => {
                const r = mean.eval({ a: 5, b: 10 });
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            });
            it('should accept vectors', () => {
                const r = vectorMean.eval({ x: vector });
                assert.closeTo(r, 3.5, 10e-9);
            });
            it('should throw if not all arguments are given', () => {
                assert.throws(() => {
                    vectorMean.eval();
                }, /wrong number of input arguments/);
            });
            it('should support expression without arguments', () => {
                assert.closeTo(pi.eval(), 3.14, 0.01);
            });
            it('should modify the vector in place', () => {
                const v = new Float64Array(10);
                vectorFill.eval(v);
                for (const i in v) assert.closeTo(v[i], +i, 10e-6);
            });
        });

        describe('evalAsync() object form', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.evalAsync({ a: 5, b: 10 }), (5 + 10) / 2, 10e-9);
            });
            it('should accept vectors', () => {
                return assert.eventually.closeTo(vectorMean.evalAsync({ x: vector }), 3.5, 10e-9);
            });
            it('should reject if not all arguments are given', () => {
                return assert.isRejected(vectorMean.evalAsync({}), /wrong number of input arguments/);
            });
            it('should support expression without arguments', () => {
                return assert.eventually.closeTo(pi.evalAsync({}), 3.14, 0.01);
            });
            it('should modify the vector in place', () => {
                const v = new Float64Array(10);
                return assert.isFulfilled(vectorFill.evalAsync(v).then(() => {
                    for (const i in v) assert.closeTo(v[i], +i, 10e-6);
                }));
            });
        });

        describe('eval() argument list form', () => {
            it('should accept variables', () => {
                const r = mean.eval(5, 10);
                assert.closeTo(r, (5 + 10) / 2, 10e-9);
            });
            it('should accept a TypedArray', () => {
                assert.instanceOf(mean, expr);
                const r = vectorMean.eval(vector);
                assert.closeTo(r, 3.5, 10e-9);
            });
            it('should throw w/ vector that is not a Float64Array', () => {
                assert.throws(() => {
                    vectorMean.eval(new Uint32Array(6) as unknown as Float64Array);
                }, /vector data must be a Float64Array/);
            });
            it('should throw w/ vector that is not the right size', () => {
                assert.throws(() => {
                    vectorMean.eval(new Float64Array(4));
                }, /size 4 does not match declared size 6/);
            });
        });

        describe('evalAsync() argument list form', () => {
            it('should accept variables', () => {
                return assert.eventually.closeTo(mean.evalAsync(5, 10), (5 + 10) / 2, 10e-9);
            });
            it('should accept an TypedArray', () => {
                return assert.eventually.closeTo(vectorMean.evalAsync(vector), 3.5, 10e-9);
            });
            it('should reject w/ vector that is not a Float64Array', () => {
                return assert.isRejected(vectorMean.evalAsync(new Uint32Array(6) as unknown as Float64Array),
                    /vector data must be a Float64Array/);
            });
            it('should reject w/ vector that is not the right size', () => {
                return assert.isRejected(vectorMean.evalAsync(new Float64Array(4)),
                    /size 4 does not match declared size 6/);
            });
            it('should work with concurrent invocations', () => {
                const big = 128 * 1024;
                const vectorMean = new expr(
                    'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                    [], { 'x': big });
                const a = [];
                for (let i = 0; i < 16; i++) {
                    a[i] = new Float64Array(big);
                    a[i].fill(i);
                }
                vectorMean.maxParallel = Math.min(os.cpus().length, 3);
                return assert.isFulfilled(Promise.all(a.map((v) => vectorMean.evalAsync(v))).then((r) => {
                    for (let i = 0; i < 16; i++)
                        assert.closeTo(r[i], i, 10e-9);
                    if (process.env.MOCHA_TEST_CONCURRENCY === undefined || +process.env.MOCHA_TEST_CONCURRENCY == 1)
                        // On real CPUs if we launch 16 128Kb arrays we should always get
                        // exactly maxParallel instances running at the same time
                        assert.equal(vectorMean.maxActive, vectorMean.maxParallel);
                    else if (os.cpus().length > 1)
                        // Virtual CPUs (Github Actions) version:
                        // if we launch 16 128Kb arrays and we have more than 1 vCPU
                        // we should get at least 2 running at the same time
                        assert.isAtLeast(vectorMean.maxActive, 2);
                }));
            });
            it('should create exactly one async context', () => {
                let asyncHooksCreated = 0;
                const asyncHook =
                    asyncHooks.createHook({
                        init: function init(asyncId, type) {
                            if (type === 'ExprTk.js:async') asyncHooksCreated++;
                        }
                    });
                asyncHook.enable();
                return assert.isFulfilled(mean.evalAsync(5, 10).then((r) => {
                    asyncHook.disable();
                    assert.equal(asyncHooksCreated, 1);
                    assert.closeTo(r, (5 + 10) / 2, 10e-9);
                }));
            });
            it('should pass a this value when used in callback mode', (done) => {
                mean.evalAsync({ a: 5, b: 10 }, function (e, r) {
                    try {
                        assert.isDefined(r);
                        if (r) assert.closeTo(r, (5 + 10) / 2, 10e-9);
                        assert.isNull(e);
                        assert.strictEqual(this, mean);
                        done();
                    } catch (e) {
                        done(e);
                    }
                });
            });
        });

        describe('map()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = clamp.map(vector, 'x', 2, 4);
                assert.instanceOf(r, Float64Array);
                assert.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            });

            it('should accept named arguments in an object ', () => {
                const r = clamp.map(vector, 'x', {minv: 2, maxv: 4});
                assert.instanceOf(r, Float64Array);
                assert.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            });

            it('should accept a preallocated array', () => {
                const dst = new Float64Array(6);
                const r = clamp.map(dst, vector, 'x', 2, 4);
                assert.strictEqual(r, dst);
                assert.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            });

            it('should support subarrays', () => {
                const r = clamp.map(vector.subarray(2, 4), 'x', 2, 4);
                assert.deepEqual(r, new Float64Array([3, 4]));
            });

            it('should evaluate an expression with multiple parallel instances', () => {
                const r = plus.map(expr.maxParallel, bigarray, 'a', 12);
                assert.instanceOf(r, Float64Array);
                assert.equal(r.length, big);
                if (process.env.MOCHA_TEST_CONCURRENCY === undefined
                        || +process.env.MOCHA_TEST_CONCURRENCY == 1
                        && os.cpus().length >= 4)
                    assert.isAbove(plus.maxActive, 1);
                for (let i = 0; i < big; i += big / 1024)
                    assert.closeTo(r[i], i + 12, 10e-9);
            });

            it('should support odd-sized subtasks', () => {
                const r = plus.map(Math.min(expr.maxParallel - 1, 1), bigarray, 'a', 12);
                assert.instanceOf(r, Float64Array);
                assert.equal(r.length, big);
                for (let i = 0; i < big; i += big / 1024)
                    assert.closeTo(r[i], i + 12, 10e-9);
            });

            it('should throw w/ invalid array', () => {
                assert.throws(() => {
                    clamp.map((new Float32Array(6)) as unknown as Float64Array, 'x', 4);
                }, /array argument must be a Float64Array/);
            });

            it('should reject if the number of threads is invalid', () => {
                assert.throws(() => {
                    plus.map(expr.maxParallel + 1, bigarray, 'a', 12);
                }, /exceed maxParallel/);
            });

            it('should throw w/o iterator', () => {
                assert.throws(() => {
                    (clamp as any).map(vector, 2, 4);
                }, /invalid iterator variable name/);
            });

            it('should throw w/ invalid iterator', () => {
                assert.throws(() => {
                    clamp.map(vector, 'c', 2, 4);
                }, /c is not a declared scalar variable/);
            });

            it('should throw w/ invalid variables', () => {
                assert.throws(() => {
                    clamp.map(vector, 'x', 4);
                }, /wrong number of input arguments/);
            });

            it('should throw w/ invalid preallocated array', () => {
                assert.throws(() => {
                    clamp.map((new Float32Array(6)) as unknown as Float64Array, vector, 'x', 4);
                }, /target array must be a Float64Array/);
            });

            it('should throw w/ preallocated array of wrong size', () => {
                assert.throws(() => {
                    clamp.map(new Float64Array(4), vector, 'x', 4);
                }, /both arrays must have the same size/);
            });
        });

        describe('mapAsync()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = clamp.mapAsync(vector, 'x', 2, 4);
                return assert.eventually.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
            });

            it('should accept a preallocated array', () => {
                const dst = new Float64Array(6);
                const q = clamp.mapAsync(dst, vector, 'x', 2, 4);
                return assert.isFulfilled(q.then((r) => {
                    assert.deepEqual(r, new Float64Array([2, 2, 3, 4, 4, 4]));
                    assert.strictEqual(r, dst);
                }));
            });

            it('should reject w/o iterator', () => {
                return assert.isRejected((clamp as any).mapAsync(vector, 2, 4),
                    /invalid iterator variable name/);
            });

            it('should reject w/ invalid iterator', () => {
                return assert.isRejected(clamp.mapAsync(vector, 'c', 2, 4), /c is not a declared scalar variable/);
            });

            it('should reject w/ invalid variables', () => {
                return assert.isRejected(clamp.mapAsync(vector, 'x', 4), /wrong number of input arguments/);
            });

            it('should throw w/ preallocated array of wrong size', () => {
                return assert.isRejected(clamp.mapAsync(new Float64Array(4), vector, 'c', 2, 4),
                    /both arrays must have the same size/);
            });

            it('should evaluate an expression with multiple parallel instances', () => {
                const q = plus.mapAsync(expr.maxParallel, bigarray, 'a', 12);
                return assert.isFulfilled(q.then((r) => {
                    assert.instanceOf(r, Float64Array);
                    assert.equal(r.length, big);
                    for (let i = 0; i < big; i += big / 1024)
                        assert.closeTo(r[i], i + 12, 10e-9);
                }));
            });

            it('should reject if the number of threads is invalid', () => {
                const q = plus.mapAsync(expr.maxParallel + 1, bigarray, 'a', 12);
                return assert.isRejected(q, /exceed maxParallel/);
            });

            it('should support writing into a preallocated array', () => {
                const dst = new Float64Array(big);
                const q = plus.mapAsync(expr.maxParallel, dst, bigarray, 'a', 12);
                return assert.isFulfilled(q.then((r) => {
                    assert.strictEqual(dst, r);
                    assert.equal(r.length, big);
                    for (let i = 0; i < big; i += big / 1024)
                        assert.closeTo(r[i], i + 12, 10e-9);
                }));
            });
        });

        describe('reduce()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = sumPow.reduce(vector, 'x', 'a', 0, 2);
                assert.isNumber(r);
                assert.equal(r, 91);
            });

            it('should support subarrays', () => {
                const r = sumPow.reduce(vector.subarray(2, 4), 'x', 'a', 0, 2);
                assert.isNumber(r);
                assert.equal(r, 25);
            });

            it('should throw w/o array', () => {
                assert.throws(() => {
                    (sumPow as any).reduce();
                }, /first argument must be/);
            });

            it('should throw w/o iterator', () => {
                assert.throws(() => {
                    (sumPow as any).reduce(vector, 2, 4);
                }, /second argument must be the iterator variable name/);
            });

            it('should throw w/ invalid iterator', () => {
                assert.throws(() => {
                    (sumPow as any).reduce(vector, 'c', 'a', 4);
                }, /c is not a declared scalar variable/);
            });

            it('should throw w/o accumulator', () => {
                assert.throws(() => {
                    (sumPow as any).reduce(vector, 'x');
                }, /third argument must be the accumulator variable name/);
            });

            it('should throw w/ invalid accumulator', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'b', 4);
                }, /b is not a declared scalar variable/);
            });

            it('should throw w/o accumulator initial value', () => {
                assert.throws(() => {
                    (sumPow as any).reduce(vector, 'x', 'a');
                }, /fourth argument must be a number for the accumulator initial value/);
            });

            it('should throw w/ invalid accumulator initial value', () => {
                assert.throws(() => {
                    (sumPow as any).reduce(vector, 'x', 'a', 'd');
                }, /fourth argument must be a number for the accumulator initial value/);
            });

            it('should throw w/ invalid variables', () => {
                assert.throws(() => {
                    sumPow.reduce(vector, 'x', 'a', 0);
                }, /wrong number of input arguments/);
            });
        });

        describe('reduceAsync()', () => {

            it('should evaluate an expression over all values of an array', () => {
                const r = sumPow.reduceAsync(vector, 'x', 'a', 0, 2);
                return assert.eventually.equal(r, 91);
            });

            it('should throw w/o iterator', () => {
                return assert.isRejected((sumPow as any).reduceAsync(vector, 2, 4),
                    /second argument must be the iterator variable name/);
            });

            it('should throw w/ invalid iterator', () => {
                return assert.isRejected((sumPow as any).reduceAsync(vector, 'c', 'a', 4),
                    /c is not a declared scalar variable/);
            });

            it('should throw w/o accumulator', () => {
                return assert.isRejected((sumPow as any).reduceAsync(vector, 'x'),
                    /third argument must be the accumulator variable name/);
            });

            it('should throw w/ invalid accumulator', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'b', 4),
                    /b is not a declared scalar variable/);
            });

            it('should throw w/o accumulator initial value', () => {
                return assert.isRejected((sumPow as any).reduceAsync(vector, 'x', 'a'),
                    /fourth argument must be a number for the accumulator initial value/);
            });

            it('should throw w/ invalid accumulator initial value', () => {
                return assert.isRejected((sumPow as any).reduceAsync(vector, 'x', 'a', 'd'),
                    /fourth argument must be a number for the accumulator initial value/);
            });

            it('should throw w/ invalid variables', () => {
                return assert.isRejected(sumPow.reduceAsync(vector, 'x', 'a', 0), /wrong number of input arguments/);
            });
        });

        describe('cwise()', () => {

            it('should accept a pre-existing array', () => {
                const result = new Float32Array(5);
                const r = density.cwise({ P, T, phi, R, Mv, Md }, result);
                assert.strictEqual(result, r);
                for (const i in result) assert.closeTo(result[i], expected[i], 10e-5);
            });

            it('should create a new array', () => {
                const result = density.cwise({ P, T, phi, R, Mv, Md });
                assert.instanceOf(result, Float64Array);
                for (const i in result) assert.closeTo(result[i], expected[i], 10e-5);
            });

            it('should support subarrays', () => {
                const result = density.cwise({
                    P: P.subarray(2, 4),
                    T: T.subarray(2, 4),
                    phi: phi.subarray(2, 4),
                    R, Mv, Md
                });
                assert.instanceOf(result, Float64Array);
                for (const i in result) assert.closeTo(result[i], expected[+i + 2], 10e-5);
            });

            it('should support using multiple parallel instances', () => {
                const result = new Float32Array(5);
                const r = density.cwise(density.maxParallel, { P, T, phi, R, Mv, Md }, result);
                assert.strictEqual(result, r);
                for (const i in result) assert.closeTo(result[i], expected[i], 10e-5);
            });

            it('should work when optimizing away the type conversion', () => {
                const r = plus.cwise({ a: bigarray, b: 14 });
                for (let i = 0; i < bigarray.length; i += 128) assert.closeTo(r[i], i + 14, 10e-5);
            });

            it('should work w/o type conversion and multiple parallel instances', () => {
                const r = plus.cwise(plus.maxParallel, { a: bigarray, b: 14 });
                for (let i = 0; i < bigarray.length; i += 128) assert.closeTo(r[i], i + 14, 10e-5);
            });

            it('should throw on missing arguments', () => {
                assert.throws(() => {
                    density.cwise({ P, T, R, Mv, Md });
                }, /wrong number of input arguments/);
            });

            it('should throw on invalid arguments', () => {
                assert.throws(() => {
                    (density as any).cwise({ P: 'abc', T, R, Mv, Md });
                }, /P is not a number or a TypedArray/);
            });

            it('should throw on invalid number of threads', () => {
                assert.throws(() => {
                    (density as any).cwise(1e3, { P, T, R, Mv, Md });
                }, /maximum threads must not exceed maxParallel/);
            });

            it('should throw on invalid variables', () => {
                assert.throws(() => {
                    (density as any).cwise({ P, T, R, Mv, Md, X: 12 });
                }, /X is not a declared scalar variable/);
            });

            it('should throw on invalid vector lengths', () => {
                assert.throws(() => {
                    (density as any).cwise({ P, T, R, Mv, Md: new Float64Array(2) });
                }, /all vectors must have the same number of elements/);
            });

            it('should throw on vector arguments', () => {
                assert.throws(() => {
                    vectorMean.cwise({});
                }, /are not compatible with vector arguments/);
            });

            it('should throw if all the arguments are scalar', () => {
                assert.throws(() => {
                    density.cwise({ P: 1, T: 2, phi: 3, R, Mv, Md });
                }, /at least one argument must be a non-zero length vector/);
            });
        });

        describe('cwiseAsync()', () => {

            it('should accept a pre-existing array', () => {
                const result = new Float32Array(5);
                const q = density.cwiseAsync({ P, T, phi, R, Mv, Md }, result);
                return assert.isFulfilled(q.then((r) => {
                    assert.strictEqual(result, r);
                    for (const i in result) assert.closeTo(result[i], expected[i], 10e-5);
                }));
            });

            it('should create a new array', () => {
                const q = density.cwiseAsync({ P, T, phi, R, Mv, Md }).then((r) => {
                    assert.instanceOf(r, Float64Array);
                    for (const i in r) assert.closeTo(r[i], expected[i], 10e-5);
                });
                return assert.isFulfilled(q);
            });

            it('should support using multiple parallel instances', () => {
                const q = density.cwiseAsync(density.maxParallel, { P, T, phi, R, Mv, Md });
                return assert.isFulfilled(q.then((r) => {
                    for (const i in r) assert.closeTo(r[i], expected[i], 10e-5);
                }));
            });

            it('should reject on missing arguments', () => {
                return assert.isRejected(density.cwiseAsync({ P, T, R, Mv, Md }),
                    /wrong number of input arguments/);
            });

            it('should reject on invalid arguments', () => {
                return assert.isRejected((density as any).cwiseAsync({ P: 'abc', T, R, Mv, Md }),
                    /P is not a number or a TypedArray/);
            });

            it('should reject on vector arguments', () => {
                return assert.isRejected(vectorMean.cwiseAsync({}),
                    /are not compatible with vector arguments/);
            });

            it('should reject if all the arguments are scalar', () => {
                return assert.isRejected(density.cwiseAsync({ P: 1, T: 2, phi: 3, R, Mv, Md }),
                    /at least one argument must be a non-zero length vector/);
            });
        });
    });

    describe('cwise()/cwiseAsync() type conversions', () => {
        const vector = [-100, 100, 10e3, 10e6];
        for (const inp of ['Uint8', 'Int8', 'Uint16', 'Int16', 'Uint32', 'Int32', 'Float32', 'Float64']) {
            for (const outp of ['Uint8', 'Int8', 'Uint16', 'Int16', 'Uint32', 'Int32', 'Float32', 'Float64']) {
                it(`converting from ${inp} to ${outp}`, () => {
                    const id = new Expression.Float64('x', ['x']);
                    const inArray = new (global as any)[inp + 'Array'](vector);
                    const outArray = new (global as any)[outp + 'Array'](vector);
                    id.cwise({ x: inArray }, outArray);
                    const size = Math.min(+((inp.match(/[0-9]+/) || [])[0]), +((outp.match(/[0-9]+/) || [])[0]));
                    for (const i in vector) {
                        if (vector[i] > 0 && vector[i] < 2 ** size)
                            assert.closeTo(vector[i], outArray[i], 10e-6);
                        if (vector[i] < 0 && !inp.startsWith('U') && !outp.startsWith('U'))
                            assert.closeTo(vector[i], outArray[i], 10e-6);
                    }
                });
            }
        }
    });
});
