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

**Type Hints & Optimizations** (`src/compiler.c`)
- Compiler tracks type hints (H_INT, H_FLOAT, H_UNKNOWN) for registers
- Generates specialized opcodes when types are known (e.g., OPC_ADDI for int+int, OPC_ADDF for float+float)
- Recent optimization work focused on prefix/postfix increment/decrement operators

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
