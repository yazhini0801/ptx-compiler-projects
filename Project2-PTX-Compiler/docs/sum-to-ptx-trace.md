# Hand trace: `sum_to.ptx`

This trace follows `examples/sum_to.k` into its generated PTX. The example calculates `total = 0 + 1 + ... + (n - 1)` for each in-range thread and stores the result at `out[i]`.

## Header and parameters

| PTX | Meaning |
| --- | --- |
| `.version 7.8` | PTX language version used by the generated file. |
| `.target sm_75` | Target GPU architecture is compute capability 7.5. |
| `.address_size 64` | Addresses are 64 bits wide. |
| `.visible .entry sum_to(...)` | Declares a kernel entry point named `sum_to`. |
| `.param .u64 sum_to_param_0` | First argument is a 64-bit pointer (`int* out`). |
| `.param .u32 sum_to_param_1` | Second argument is a 32-bit integer (`int n`). |
| `.reg ...` declarations | Reserves virtual predicate, integer, and address registers used below. |

## Load arguments and calculate the thread index

| PTX | Meaning |
| --- | --- |
| `ld.param.u64 %rd2, [sum_to_param_0];` | Loads the `out` pointer from the kernel parameter area. |
| `cvta.to.global.u64 %rd1, %rd2;` | Converts that generic address into a global-memory address. |
| `ld.param.u32 %r1, [sum_to_param_1];` | Loads `n`. |
| `mov.u32 %r3, %ctaid.x;` | Reads the block's x index. |
| `mov.u32 %r4, %ntid.x;` | Reads the number of threads in the block. |
| `mov.u32 %r5, %tid.x;` | Reads this thread's x index within its block. |
| `mad.lo.s32 %r6, %r3, %r4, %r5;` | Computes `block_index * block_size + thread_index`; this is `gid()`. |
| `mov.b32 %r2, %r6;` | Copies the calculated index into the register used for source variable `i`. |

## Bounds check and initialize the loop variables

| PTX | Meaning |
| --- | --- |
| `setp.lt.s32 %p1, %r2, %r1;` | Sets `%p1` when `i < n`. |
| `@!%p1 bra L_end1;` | If `i >= n`, skip the work and go to the kernel end. |
| `mov.s32 %r8, 0;` / `mov.b32 %r7, %r8;` | Initializes `total` to zero. `%r7` represents `total`. |
| `mov.s32 %r10, 0;` / `mov.b32 %r9, %r10;` | Initializes loop counter `k` to zero. `%r9` represents `k`. |

## `while (k < n)` loop

| PTX | Meaning |
| --- | --- |
| `L_cond2:` | Label at the top of the loop; branches return here. |
| `setp.lt.s32 %p2, %r9, %r1;` | Tests the loop condition `k < n`. |
| `@!%p2 bra L_end2;` | Exit when `k >= n`. |
| `add.s32 %r11, %r7, %r9;` | Computes `total + k`. |
| `mov.b32 %r7, %r11;` | Updates `total`. |
| `mov.s32 %r12, 1;` | Creates the constant one. |
| `add.s32 %r13, %r9, %r12;` | Computes `k + 1`. |
| `mov.b32 %r9, %r13;` | Updates `k`. |
| `bra L_cond2;` | Jumps back to test the loop condition again. |
| `L_end2:` | Loop exit label. |

## Store the result and return

| PTX | Meaning |
| --- | --- |
| `mul.wide.s32 %rd3, %r2, 4;` | Converts integer index `i` to a byte offset (`i * sizeof(int)`). |
| `add.s64 %rd4, %rd1, %rd3;` | Adds that offset to the global `out` pointer. |
| `st.global.s32 [%rd4], %r7;` | Stores `total` at `out[i]`. |
| `L_end1:` | Label reached by threads that fail the initial bounds check. |
| `ret;` | Ends this thread's execution of the kernel. |

## Interview-sized summary

The frontend turns `gid()`, comparisons, a `while` loop, and an indexed store into typed AST nodes. The PTX generator maps the thread index to integer registers, comparisons to `setp` predicates, loop control to a condition label and branches, and `out[i]` to address arithmetic plus a global store. Project 1 can then identify the loop's back edge from the generated control-flow graph.
