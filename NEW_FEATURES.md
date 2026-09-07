# New Features Implementation Summary

## ✅ Completed Features (7/7)

### 1. String Interpolation

Embed variables directly in strings using `${variable}` syntax:

```fh
let name = "Alice";
let age = 30;
let message = "Hello, ${name}! You are ${age} years old.";
# Result: "Hello, Alice! You are 30 years old."
```

**Implementation:**
- Parser detects `${...}` in string literals
- Converts to concatenation: `"Hello, " + name + "! You are " + age + " years old."`
- Works with any variable name
- Zero runtime overhead

**Files Modified:** `src/parser.c`

---

### 2. JSON Support

Full JSON parsing and stringification support:

```fh
# Parse JSON string to FH values
let data = json_parse('{"name": "John", "age": 30, "scores": [95, 87, 92]}');
print(data["name"]);  # "John"
print(data["scores"][0]);  # 95

# Convert FH values to JSON string
let person = {
    "name": "Alice",
    "age": 25,
    "active": true
};
let json_str = json_stringify(person);
# Result: '{"name":"Alice","age":25,"active":true}'
```

**Supported Types:**
- Objects (FH maps) ↔ JSON objects
- Arrays ↔ JSON arrays
- Strings ↔ JSON strings
- Numbers (int/float) ↔ JSON numbers
- Booleans ↔ JSON booleans
- Null ↔ JSON null

**Implementation:**
- Integrated cJSON library (single-file, MIT licensed)
- Added `json_parse(string)` and `json_stringify(value)` functions
- Recursive conversion between FH and JSON types

**Files Added:** `src/cJSON.{c,h}`
**Files Modified:** `src/c_funcs.c`, `Makefile`

---

### 3. Assert with Custom Messages

Enhanced assertions with descriptive error messages:

```fh
let x = 10;
assert(x > 0, "x must be positive");
assert(x < 100, "x must be less than 100");

# On failure, shows:
# "assert() failed: x must be positive (cond=bool false)"
```

**Implementation:**
- Already existed in the codebase
- Verified and tested functionality
- Supports optional second parameter for custom message

**Files:** `src/c_funcs.c` (existing functionality)

---

### 4. Better Error Messages with Suggestions

Intelligent error suggestions for typos:

```fh
fn main() {
    let length = 10;
    let width = 20;
    let area = lenght * width;  # Typo!
}
```

**Error Output:**
```
ERROR: unknown variable or function 'lenght'. Did you mean 'length'?
```

**Implementation:**
- Uses Levenshtein distance algorithm
- Searches local variables in current scope
- Suggests if edit distance ≤ 2
- Helps catch common typos instantly

**Files Modified:** `src/compiler.c`

---

### 5. Default Function Parameters

Allow functions to have optional parameters with default values:

```fh
fn greet(name, greeting = "Hello") {
    printf("%s, %s!\n", greeting, name);
}

greet("Alice");           # Uses default: "Hello, Alice!"
greet("Bob", "Hi");       # Overrides: "Hi, Bob!"

fn add(a, b = 10, c = 20) {
    return a + b + c;
}

add(5, 3, 2);  # Returns 10
add(5, 15);    # Returns 40 (5 + 15 + 20)
add(7);        # Returns 37 (7 + 10 + 20)
```

**Implementation:**
- Extended AST (`src/ast.h`) to add `default_values` field to store default expressions
- Modified parser (`src/parser.c`) to recognize `param = default_expr` syntax
- Updated compiler (`src/compiler.c`) to generate null-checking bytecode at function entry
- Uses `OPC_CMP_EQ` and `OPC_JMP` to conditionally assign defaults only when parameters are null
- VM already initializes missing parameters to null in `prepare_call` function

**Files Modified:** `src/ast.h`, `src/parser.c`, `src/compiler.c`

---

### 6. Optional Chaining

Safe property access that returns null if intermediate values are null/missing:

```fh
let user = {
    "profile": {
        "address": {
            "city": "New York"
        }
    }
};

# Safe access - no error if intermediate values are null
let city = user?.["profile"]?.["address"]?.["city"];  # "New York"

let missing_user = null;
let result = missing_user?.["profile"];  # null (no error)

let partial = {"name": "Alice"};
let no_profile = partial?.["profile"]?.["address"];  # null
```

**Implementation:**
- Added `?.` operator to tokenizer and operator table
- Extended AST with `EXPR_OPTIONAL_INDEX` expression type
- Modified parser to recognize `?.[...]` syntax (similar to regular `[...]` indexing)
- Compiler generates null-checking bytecode using CMP_EQ and conditional jumps
- Pattern: Check if container is null → if yes, return null; if no, perform indexing

**Files Modified:** `src/ast.h`, `src/operator.c`, `src/parser.c`, `src/compiler.c`, `src/ast.c`

---

### 7. `pcall` — catching an error instead of dying

FH has no `try`/`catch`, and it does not need one to be able to recover: an
error is a return code all the way up, so a protected call is enough.

```fh
let r = pcall(load_level, "level3.json");
if (r.ok) {
    start(r.value);
} else {
    println("could not load the level: " + r.error);
    println(r.traceback);
    start(default_level());
}
```

`pcall(f [, args...])` always returns a map:

```
{ ok: true,  value: <what f returned>, error: null }
{ ok: false, value: null, error: "<message>",
  file: "...", line: n, col: n, traceback: "..." }
```

- `f` may be a script function, an anonymous one, or a **C function** the host
  registered — a binding that returns -1 is caught the same way, which is the
  case that matters when FH is embedded in an engine.
- Wrap an expression with `pcall(fn() { ... })`.
- It nests, and its own misuse (`pcall(42)`) is itself catchable.
- There is no `finally`: undo work yourself after looking at `ok`.

`error(x)` now takes any value, not only a string — `error(404)` and
`error({"code": 404})` render into the message rather than being replaced by
a complaint about the argument's type.

**Implementation:** `fn_pcall` in `src/c_funcs.c`,
`fh_unwind_vm_call_stack()` in `src/vm.c`.

**Files Modified:** `src/c_funcs.c`, `src/vm.c`, `src/vm.h`, `src/program.c`,
`src/fh_internal.h`, `src/gc.c`

---

## All Features Completed! 🎉

---

## Testing

All features are tested in `tests/test_new_features.fh`:

```bash
./fh tests/test_new_features.fh
```

Full test suite (all passing):

```bash
./run_tests.sh
```

---

## Performance Impact

- **String Interpolation:** Zero runtime overhead (compile-time transformation)
- **JSON Support:** Minimal (~500 lines, cJSON is highly optimized)
- **Error Suggestions:** Compile-time only (no runtime cost)
- **Assert Messages:** Negligible (only on assertion failure)

---

## Credits

Features implemented with assistance from Claude Sonnet 4.5 (Anthropic).

JSON support uses [cJSON](https://github.com/DaveGamble/cJSON) by Dave Gamble (MIT License).
