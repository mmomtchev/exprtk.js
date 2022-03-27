# A few notes on performance

## V8 `TypedArray.prototype.map`

V8 has an obvious performance problem with `TypedArray.prototype.map` which I haven't investigated.

## V8 function calls

As one can see from the benchmark, function calls not only are next to free, they can in fact lead to increased performance. The reason is that V8 will be more keen to compile/optimize an isolated function than a loop in a larger function. When it comes to the function call itself - modern CPUs are very good at branch prediction and prefetching when they have to deal with a single `CALL` instruction - which means they will be capable to execute the subroutine almost in the same way as if it was inlined. There is a common misconception that V8 is very good at inlining functions - in fact V8 is simply good at producing subroutines in a way that allows CPU prefetching to work.

## 2 * cos(x) / sqrt(x)

This function has been chosen as an example of a function where JIT compilation doesn't play such an important role as both `cos` and `sqrt` are performed by very well optimized subroutines both in V8 and in `ExprTk`. Also, given the more complex nature of the function, this test is less dependant on cache bandwidth and allows for parallelization to have greater impact.

## x² + 2x + 1

This function has been chosen as an example of a function that is very well optimized both by V8.

### In conjunction with `cwise` from `scijs/ndarray`

When used in conjunction with `cwise` of `scijs/ndarray` and 64-bit floating point numbers, V8 produces an absolutely unbeatable code for this function, on par with hand-optimized assembly:

```
movapd %xmm1,%xmm2
addsd  %xmm1,%xmm2
movapd %xmm1,%xmm3
mulsd  %xmm1,%xmm3
addsd  %xmm2,%xmm3
lea    (%r11,%rax,1),%rdx
addsd  %xmm0,%xmm3
movsd  %xmm3,(%rdx,%r15,8)
```

Loading the floating point number is performed in the most efficient way possible (`MOVAPSD`), the number is then squared, the multiplication by two has been transformed to 2 additions, everything is interleaved to take advantage of the super-scalar pipeline, and the increment by one is performed by using a constant held throughout the loop in the `xmm0` register - which means that there was register optimization. `gcc`/`clang` produce a near-identical code for the same function when invoked with `-O3`, leaving `ExprTk` in the dust. In fact, when performing this function, the array traversal accounts for 90% of the spent CPU time with the actual calculation being 10%.

## V8

Because JS does not have pointers, V8 has a sub-optimal array traversal, computing the array offset - every array offset - from the array start and the index at every iteration. This is further aggravated by the lack of (good) register optimization which means that the array index is stored in memory (usually on the stack) and not in a register. Modern CPU caches partially alleviate this problem. Exception handling is very efficient and it is not a huge factor - optimized code runs almost as fast with bounds checking than without.

You can also check this Medium article which discusses in detail the code produced by V8 for simple array traversal: <https://mmomtchev.medium.com/in-2021-is-there-still-a-huge-performance-difference-between-javascript-and-c-for-cpu-bound-8ff798d999d6>.

## ExprTk

ExprTk also generates very good machine code for the given function. It comes down to a single master `CALL` from `expression.cc` to the first control block in the template which in turn calls the polynomial evaluator. Most of the performance loss, compared to the inlined code in the V8 case, comes from having to save and reload all the values from memory before and after each `CALL` - and also from the `CALL` itself - since this `CALL` will be a dynamic call with the value held in a register - potentially stalling the CPU pipeline. The inline code produced by V8 is able to keep some values in a register. This can't be achieved without *fusing* the expression evaluation into the loop logic.

Also, the way the `ExprTk` symbol table works - holding a reference and not a pointer - `ExprTk.js` has to generate an extra store instruction which could have been avoided at every access of the iterator array. This problem could eventually be addressed in the future.
