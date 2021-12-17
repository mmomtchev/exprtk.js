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
        it('should throw w/ vector that is not a TypedArray', () => {
            assert.throws(() => {
                new expr('a', ['a'], {x: 12});
            }, /vector data must be a Float64Array/)
        })
        it('should throw w/ vector that is not a Float64Array', () => {
            assert.throws(() => {
                new expr('a', ['a'], { x: new Uint32Array(1) });
            }, /vector data must be a Float64Array/)
        })
        it('should accept an expression w/ variables', () => {
            const mean = new expr('(a + b) / 2', ['a', 'b']);
            assert.instanceOf(mean, expr);
        })
    })

    describe('eval()', () => {
        let mean;
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
        })
        it('should accept variables', () => {
            const r = mean.eval({ a: 5, b: 10 });
            assert.closeTo(r, (5 + 10) / 2, 10e-9);
        })
    })

    describe('evalAsync()', () => {
        let mean;
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
        })
        it('should accept variables', () => {
            return assert.eventually.closeTo(mean.evalAsync({ a: 5, b: 10 }), (5 + 10) / 2, 10e-9);
        })
    })

    describe('fn()', () => {
        let mean;
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
        })
        it('should accept variables', () => {
            const r = mean.fn(5, 10);
            assert.closeTo(r, (5 + 10) / 2, 10e-9);
        })
        it('should accept an TypedArray', () => {
            const mean = new expr(
                'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                [], { 'x': new Float64Array([1, 2, 3, 4, 5, 6]) });
            assert.instanceOf(mean, expr);
            const r = mean.fn();
            assert.closeTo(r, 3.5, 10e-9);
        })
    })

    describe('fnAsync()', () => {
        let mean;
        before(() => {
            mean = new expr('(a + b) / 2', ['a', 'b']);
        })
        it('should accept variables', () => {
            return assert.eventually.closeTo(mean.fnAsync(5, 10), (5 + 10) / 2, 10e-9);
        })
        it('should accept an TypedArray', () => {
            const mean = new expr(
                'var r := 0; for (var i := 0; i < x[]; i += 1) { r += x[i]; }; r / x[];',
                [], { 'x': new Float64Array([1, 2, 3, 4, 5, 6]) });
            assert.instanceOf(mean, expr);
            return assert.eventually.closeTo(mean.fnAsync(), 3.5, 10e-9);
        })
    })
})
