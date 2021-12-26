# A few notes on performance

## V8 `TypedArray.prototype.map`

V8 has an obvious performance problem with `TypedArray.prototype.map`. I haven't investigated it, but I strongly suspect that the function is megamorphic and its optimization does not work.

## V8 function calls

As one can see from the benchmark, function calls not only are next to free, they can in fact lead to increased performance. The reason is that V8 will be more keen to compile/optimize an isolated function than a loop in a larger function. When it comes to the function call itself - modern CPUs are very good at branch prediction and prefetching when they have to deal with a single `CALL` instruction - which means they will be capable to execute the subroutine almost in the same way as if it was inlined. There is a common misconception that V8 is very good at inlining functions - in fact V8 is simply good at producing subroutines in a way that allows CPU prefetching to work.

## x² + 2x + 1

The performance function chosen for the benchmark is a function that is very well optimized both by V8 and ExprTK. Both recognize its polynomial form and generate very efficient machine code that comes relatively close to the maximum achievable on the given hardware without the use of SIMD instructions.

## V8

Because JS does not have pointers, V8 has a sub-optimal array traversal, computing the array offset - every array offset - from the array start and the index at every iteration. This is further aggravated by the lack of (good) register optimization which means that the array index is stored in memory (usually on the stack) and not in a register. Modern CPU caches partially alleviate this problem. Exception handling is very efficient and it is not a huge factor - optimized code runs almost as fast with bounds checking than without.

You can also check this Medium article which discusses in detail the code produced by V8 for simple array traversal: <https://mmomtchev.medium.com/in-2021-is-there-still-a-huge-performance-difference-between-javascript-and-c-for-cpu-bound-8ff798d999d6>.

## ExprTk

ExprTk also generates very good machine code for the given function. It comes down to a single master `CALL` from `expression.cc` to the first control block in the template which in turn calls the polynomial evaluator. The problem is that modern CPUs are not as good with two cascading `CALL` instructions in a tight loop as they are with only one. Also, the way the symbol table works - holding a reference and not a pointer - `ExprTk.js` has to generate an extra load instruction which could have been avoided at every access of the iterator array. Both problems could eventually be addressed in the future.
