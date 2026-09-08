# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FH is a dynamically-typed scripting language designed to be embedded in native applications (particularly game engines). It compiles to bytecode and runs on a register-based virtual machine with automatic garbage collection. The implementation is ~10k lines of C99 with no external runtime dependencies.

## Build Commands

```bash
# Build the interpreter (release mode, default)
make -j2

# Run a test/script
./fh tests/mandelbrot.fh

# Build in debug mode
make TARGETS=debug

# Build with address sanitizer
make TARGETS=asan

# Clean build artifacts
make clean

# Run valgrind memory check
make check

# Install to system (Linux/macOS)
make install

# Build dynamic library for testing
make test_dynamic_lib
```

## Testing

Tests are located in `tests/` directory with `.fh` extension. Run tests by executing:

```bash
./fh tests/test_name.fh
```

Key test categories:
- `tests/benchmarks/` - Performance benchmarks (mandel, function_calls, game_logic, object_creations)
- `tests/test_*.fh` - Feature-specific tests (math, map, regex, gc, crypto, etc.)
- `tests/dynamic_libraries/` - Dynamic library integration tests

## Architecture

### Compilation Pipeline

The language follows a classic interpreter architecture:

1. **Tokenizer** (`src/tokenizer.c`) - Lexical analysis, converts source text to tokens
2. **Parser** (`src/parser.c`) - Syntax analysis, builds Abstract Syntax Tree (AST)
3. **Compiler** (`src/compiler.c`) - Bytecode generation from AST
4. **VM** (`src/vm.c`) - Register-based bytecode execution

### Key Components

**Value System** (`src/value.h`, `src/value.c`)
- All values use tagged union representation (`struct fh_value`)
- Types: null, boolean, float, integer, c_func, string, array, map, closure, c_obj, func_def
- Objects (strings, arrays, maps, closures, etc.) are heap-allocated and GC-managed
- Non-objects (null, bool, numbers, c_func) are stored inline in the value struct

**Virtual Machine** (`src/vm.c`)
- Register-based (faster dispatch than stack-based)
- Maximum 256 registers per function (`MAX_FUNC_REGS`)
- Instructions are 32-bit words with fields: OPCODE (6 bits), RA (8 bits), RB (9 bits), RC (9 bits)
- See `src/bytecode.h` for instruction encoding details and all opcodes

**Garbage Collector** (`src/gc.c`)
- Simple mark-and-sweep algorithm (tracing collector)
- Stops the world during collection
- Configurable threshold via `gc_frequency()` function
- Can be paused/resumed with `gc_pause()`
- Force collection with `gc()`
- Re-entry is refused (`prog->gc_running`): sweeping a `c_obj` runs the
  host's free callback, and if that allocates it must not start a second
  collection over a half-marked heap.

*Roots*, all in `mark_roots()`. Miss one and nothing goes wrong until much
later, in unrelated code, on maybe one run in five — so treat this list as
the invariant it is:
- `global_funcs_map`, `global_vars_map`, `pinned_objs`, `c_vals`,
  `open_upvals`, and `vm.char_cache` (pinned at startup).
- `prog->globals_init`, the compiled `<globals>` closure between compiling
  and running a chunk's initializers.
- **Every live call frame's `closure`.** A running closure is usually
  reachable some other way (a callee sits in the caller's register, a named
  function is in `global_funcs_map`), but `fh_call_vm_function()` puts its
  closure only in the frame, so a caller holding no other reference has the
  running function's own constants collected under it.
- **The VM stack, up to the highest `stack_top` of *any* live frame** — not
  the top frame's. `stack_top` is not monotonic with depth: a C-call frame's
  is `base + n_args`, which for a no-argument builtin sits below every
  register its caller is still using. Using the top frame's bound freed the
  caller's live locals on any collection triggered from inside an allocating
  builtin, and the stale slots then crashed the *next* collection.

Two pinning mechanisms exist and they are not interchangeable.
`fh_make_object(pinned=true)` pushes the object onto `pinned_objs`, which is
a **root**: the object is marked, and so are its children.
`GC_PIN_OBJ()`/`GC_UNPIN_OBJ()` set a bit that only makes `sweep()` keep the
object — it is never traversed, so anything reachable *only* through a
bit-pinned object is still collected. That is safe where it is used today
(`op_CLOSURE`, `op_NEWARRAY`, `op_NEWMAP` pin a half-built object whose
children are separately reachable from registers) and a trap anywhere else.

*Testing the collector.* `make TARGETS=gcdebug` builds with `-DFH_GC_DEBUG`:
asan, plus a root verifier that checks before every mark that each root still
points at a live object, plus poisoning of small-pool blocks on both
allocation and free. The verifier turns "segfaults one run in five" into a
deterministic labelled report on stderr; the poison turns a field an
allocator forgot to initialise into a wild pointer instead of the previous
object's plausible-looking one. `run_tests.sh` fails any test whose
output contains `GC ERROR` or `**** ERROR`, so a test cannot pass by printing
"ok" on a heap it has already corrupted. `tests/test_gc_stress.fh` leans on
the collector from every direction and pins the regressions above; run the
whole suite under `gcdebug` after touching `gc.c`, `vm.c` or `value.c`.

Note that `eval()` runs its code in a *separate* `fh_program` and hands the
result back across, so a live value can legitimately be owned by another
program's heap (kept alive for the process lifetime in
`fh_programs_vector`). The verifier knows about this; new root checks must
too.

*Writing a C function that allocates.* `fh_new_string`/`fh_new_array`/
`fh_new_map` anchor what they build in `prog->c_vals`, which the VM releases
on its next dispatch — the anchor is meant to last exactly one bytecode
instruction. A C function that calls **back into the script**
(`fh_call_vm_function`, as `pcall()` and `sort()` do) runs a nested
`fh_run_vm`, and that loop dispatches. It only releases anchors added since
*it* started (`c_vals_floor`), so values the outer C function had already
built stay anchored; without that floor they are collected while it is still
using them. If a C function instead holds a value across a callback in some
other way, it is responsible for keeping it reachable itself
(`fh_get_pin_state`/`fh_restore_pin_state`, or a VM stack slot).

**Type Hints & Optimizations** (`src/compiler.c`)
- Compiler tracks type hints (H_INT, H_FLOAT, H_UNKNOWN) for registers
- Generates specialized opcodes when types are known (e.g., OPC_ADDI for int+int, OPC_ADDF for float+float)
- Recent optimization work focused on prefix/postfix increment/decrement operators

**Error handling**
- Errors are return codes, not `longjmp`: `fh_set_error()` fills
  `prog->last_error_msg`, returns -1, and every layer propagates the -1.
  `fh_run_vm()` returns -1 with the failed call's frames still on the stack,
  which is what makes the traceback in `fh_get_error()` possible.
- `pcall()` (`fn_pcall` in `src/c_funcs.c`) is built on exactly that: it
  records the call-stack depth, runs the call, and on -1 reads the message and
  location, renders the traceback, then puts the VM back with
  `fh_unwind_vm_call_stack()`. Two things are easy to forget when touching it:
  `fh_set_error()` also clears the global `fh_running`, which several builtins
  read as "an argument conversion failed", and the value the call returned
  sits in a stack slot the collector no longer walks — hence the GC pause
  while the result map is assembled.
- A C function that calls back into the script runs with a **C-call frame**
  (`closure == NULL`) on top. Anything deriving a register window from
  `frame->closure` has to handle that; `stack_top` is the bound that is right
  for both frame kinds.

**Standard Library** (`src/c_funcs.c`, `src/functions.c`)
- Built-in functions exposed to scripts
- Crypto: bcrypt, md5 hashing (`src/crypto/`)
- Random: mt19937 (Mersenne Twister) generator
- Regex support (`src/regex/re.c`)
- TAR archive support (`src/tar/microtar.c`)
- Map/hashmap implementation (`src/map/map.c`)

### Memory Management

**AST & Symbol Table** (`src/ast.c`, `src/symtab.c`)
- AST built during parsing, used for compilation
- Symbol table manages variable names and scopes
- Source location tracking for error reporting (`src/src_loc.c`)

**Input System** (`src/input.c`)
- Abstract input interface for reading source code
- Supports files, strings, and TAR package archives (.fhpack)
- Include system for multi-file programs

### File Structure

Core interpreter files in `src/`:
- `main.c` - Entry point and script execution orchestration
- `program.c/h` - Program state container
- `fh.h` - Public API header
- `fh_internal.h` - Internal shared definitions

Supporting utilities:
- `buffer.c` - Dynamic buffer implementation
- `stack.c` - Stack data structure
- `util.c` - Utility functions
- `operator.c` - Operator handling
- `dump_ast.c`, `dump_bytecode.c` - Debug dumping

External libraries (embedded):
- `src/vec/vec.c` - Dynamic array implementation
- `src/map/map.c` - Hashmap implementation
- Third-party crypto and compression in respective subdirectories

## Language Features

- Full closures with proper upvalue support
- Dynamic typing with type coercion
- First-class functions
- Heterogeneous arrays and maps
- C function integration (`fh_add_c_func`)
- Package system (.fhpack TAR archives)
- Regex pattern matching
- Bitwise operations (AND, OR, XOR, shifts)
- Control flow: if/elif/else, while, repeat/until, for, break, continue

## Common Development Patterns

When modifying the VM:
- Instructions use RK encoding: values < 256 are registers, values >= 257 are constant pool indices
- Use `RK_IS_REG()`, `RK_IS_CONST()` macros to distinguish
- Type hints propagate through compilation to enable specialized opcodes

When adding built-in functions:
- Implement in `src/c_funcs.c` with signature `int func(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)`
- Register via `fh_add_c_func()` or `fh_add_c_funcs()`

When working on bytecode:
- See instruction encoding macros in `src/bytecode.h` (MAKE_INSTR_A, MAKE_INSTR_AB, MAKE_INSTR_ABC, etc.)
- Use `fh_dump_bytecode()` to inspect generated code
- Opcodes defined in `enum fh_bc_opcode`

## Performance Considerations

The codebase has recent optimization work on prefix/postfix increment/decrement (see git history). When making changes:
- Consider type-specific opcode variants for hot paths
- Register allocation impacts performance significantly
- The VM dispatch loop is performance-critical (src/vm.c)
- Mark-and-sweep GC pauses scale with heap size

## Platform Support

Tested on:
- Linux (GCC)
- macOS (Clang)
- Windows (MinGW)
- BSD variants (OpenBSD, FreeBSD)

Platform-specific handling in Makefile (compiler selection, library flags).
