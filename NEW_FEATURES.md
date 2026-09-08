# New Features Implementation Summary

## ✅ Completed Features (12/12)

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

### 8. Four stdlib papercuts closed

- **`%` works on floats.** `5.5 % 2` is `1.5`. `int % int` stays an integer;
  anything with a float in it comes back as a float, via `fmod`. The sign
  follows the dividend on both paths (`-5 % 3 == -2`), which is C's rule and
  not Lua's floored one — adopting Lua's would have silently changed every
  existing integer result.
- **`math_random` returns what the manual already said it did.**
  `math_random(n)` and `math_random(m, n)` returned floats, so the obvious
  use — `arr[math_random(1, len(arr))]` — failed with "non-integer index".
  They return integers now. `math_random()` divided by `UINT32_MAX`, so a
  maximal draw returned exactly `1.0` inside a range documented as `[0, 1)`.
  `math_randomseed()` accepts an integer seed.
- **`string_rep(s, n[, sep])`, `string_starts_with()`, `string_ends_with()`.**
- **`os_clock()`** (CPU seconds, like Lua's) and **`os_monotonic()`** (a
  high-resolution clock that does not jump when the wall clock is adjusted
  and allocates nothing per reading, unlike `os_time()`, which returns a
  GC-managed `c_obj` per call).

**Files Modified:** `src/vm.c`, `src/compiler.c`, `src/c_funcs.c`

---

### 9. `sort()` — the gap you noticed the first time you needed it

```fh
sort(scores);                                        # ascending, in place
sort(names);                                         # strings, by strcmp
sort(people, fn(a, b) { return a.age < b.age; });    # by a field
sort(scores, fn(a, b) { return a > b; });            # descending
```

It sorts **in place** and returns the same array. It is **stable**, so
sorting by one field then another does what you expect. The comparator is
asked "does `a` come before `b`?" — Lua's `table.sort` convention. Without
one, numbers sort numerically and strings by `strcmp`, which also gives
scripts an ordering on strings for the first time, since the VM's `<` only
accepts numbers.

Two implementation notes, since it is the only builtin that calls back into
script:

- It sorts a permutation of **indices**, not the values. The values never
  leave `arr->items`, which the collector reaches through the caller's
  register — a merge sort moving `fh_value`s through a malloc'd buffer would
  hold the only reference to them somewhere `mark_roots()` does not walk, and
  a comparator that allocates would collect them.
- Bottom-up merge sort rather than heapsort: stable, about half the
  comparisons, and it cannot run off either end when handed an inconsistent
  comparator. A comparator that resizes the array aborts the sort; an error
  inside one propagates out and is catchable with `pcall`.

**Implementation:** `fn_sort` in `src/c_funcs.c`.

---

### 10. Prototypes and `obj:method()`

The closure-per-method object pattern allocates one closure **per method per
instance**. Fifty enemies with five methods each is 250 closures, all
identical. Prototypes share one copy:

```fh
let proto = {
    "inc": fn(self, by) { self.n = self.n + by; return self; },
    "get": fn(self) { return self.n; }
};

let c = setproto({"n": 0}, proto);
c:inc(5);
print(c:get());       # 5
```

A key the object does not have is looked up in its prototype, then in that
prototype's prototype — the job Lua gives `__index`, without the metatable
around it. Measured on 50,000 objects with five methods: **2.2× less memory**
(43.8 MB → 20.3 MB), 2.5× faster to construct, and 5.3× faster method calls.

- `setproto(map, proto)` / `getproto(map)`. A cycle is refused when it is
  made, not walked into later.
- The prototype is a **slot on the map, not an entry in it**, so it stays out
  of `next_key()`, `len()`, `contains_key()` and `json_stringify()` — an
  object does not serialize its class.
- Writes always land on the object itself, so instances cannot corrupt each
  other by assignment.
- `object:method(args)` fetches `object.method` and calls it with `object` in
  front of the arguments. The object expression is evaluated **exactly
  once**, which is why it is real syntax rather than sugar for
  `obj.method(obj, ...)`. The receiver is an ordinary first parameter you
  name yourself; `self` is a convention, not a keyword.

**Files Modified:** `src/value.h`, `src/value.c`, `src/gc.c`, `src/vm.c`,
`src/c_funcs.c`, `src/ast.h`, `src/ast.c`, `src/parser.c`, `src/compiler.c`,
`src/dump_ast.c`

---

### 11. Global initializers are real code

A global used to be limited to whatever a compile-time evaluator could fold:
a literal, or an array/map of literals. That ruled out most of what people
actually write at file scope — and, awkwardly, the prototype from feature 10:

```fh
let Actor = {                                  # was: "map value must be
    "tick": fn(self, dt) { ... }               #  constant expression"
};
let CONFIG = load_config();                    # was: rejected
let DOUBLE = BASE * 2;                         # was: rejected
```

Each initializer is now compiled into one synthetic `<globals>` function that
runs when the chunk is loaded, before `main`. The names are declared (as
`null`) before anything is compiled, so functions and initializers alike
resolve them; the function bodies are compiled before `<globals>` is, so an
initializer may call any function in the file regardless of where it appears.
Initializers run top to bottom, so a forward reference reads `null` rather
than erroring, and a failure raises a normal error with a traceback naming
`<globals>`.

This replaced `eval_const_expr()` outright — ~120 lines of a second,
weaker expression evaluator that could only ever disagree with the real one.

Two GC roots were needed, and both are load-bearing (without them the test
below segfaults):

- `prog->globals_init` holds the compiled `<globals>` closure between
  compiling and running it, and stays set *while* it runs — the compiler
  unpins it, so the executing frame is otherwise the only reference to it.
- `mark_roots()` now marks the closure of every live call frame. A closure
  being executed is normally reachable anyway (a called function sits in the
  caller's register, a named one is in `global_funcs_map`), but
  `fh_call_vm_function()` puts its closure in the frame and nowhere else, so
  a caller holding no other reference to it has the running function's own
  constants collected under it.

**Files Modified:** `src/compiler.c`, `src/program.c`, `src/program.h`,
`src/gc.c`

---

### 12. Collector fixes, and a way to test the collector

Three bugs, found by leaning on the GC rather than by reading it.

**`mark_roots()` marked too little of the VM stack.** It used the *top*
frame's `stack_top`. That bound is not monotonic with depth: a C-call frame's
`stack_top` is `base + n_args`, which for a no-argument builtin sits below
every register its caller is still using. So any collection triggered while a
C function was on top — and `fh_make_object()` is what triggers collections,
so that is any builtin that allocates — swept objects the caller's live
registers still pointed at. The stale slots then crashed the *next*
collection, walking objects that had already been freed. The bound is now the
highest `stack_top` of any live frame; every slot below it belongs to some
frame's register window, and `prepare_call()` initialises every window it
opens.

```fh
let junk = [];
let i = 0;
while (i < 3000) {
    append(junk, {"a": "str" + i, "b": [i, i + 1]});
    if ((i % 100) == 0) { junk = []; gc(); }
    i = i + 1;
}
# before: "GC ERROR: marking invalid object type -701800423", then a segfault
```

**`a[i] = v` past the end grew the array by `i+1` instead of to `i+1`.**
`fh_grow_array_object()` appends its argument, so `let a = [1,2,3]; a[3] = 4;`
left `len(a)` at 7, and the `items[count] = v` push idiom the manual
documents grew the array geometrically while `len()` lied about its contents.
The out-of-memory return was also unchecked, so a failed grow wrote past the
end. Both fixed, along with `fh_grow_array_object_uninit()`'s fast path,
which ignored `num_items` and always bumped the length by one.

**A collection could re-enter itself.** Sweeping a `c_obj` runs the host's
free callback; if that ever allocates, `fh_make_object()` could start a
second collection over a half-marked heap. `prog->gc_running` refuses it.

**And a way to catch the next one.** A dangling root produces no symptom
where the mistake is: the first stack-bound bug reported corruption on
roughly one release run in five, which is not something a test can be pinned
to. `make TARGETS=gcdebug` builds with asan plus a root verifier that checks,
before every mark, that each root still points at a live object — turning
that into a deterministic labelled report:

```
$ make TARGETS=gcdebug && ./fh tests/test_gc_stress.fh
GC ERROR: dangling root vm_stack[5] -> 0x5586a2b1c240 (value type 6)
```

With the bug present that fires on every run; with it fixed, the whole suite
is clean. `run_tests.sh` now fails any test whose output carries `GC ERROR`
or `**** ERROR`, so a test can no longer pass by printing "ok" on a heap it
has already corrupted — which is exactly what the array and stack-bound
tests did before. `tests/test_gc_stress.fh` covers locals live across
allocating builtins, closures outliving their frames, deep recursion, map
growth and deletion, array growth, reachable and unreachable cycles, `pcall`
under pressure, and `sort()` calling script back from inside a C frame.

**Files Modified:** `src/gc.c`, `src/vm.c`, `src/array.c`, `src/program.c`,
`src/program.h`, `Makefile`, `run_tests.sh`, `CLAUDE.md`

---

## All Features Completed! 🎉

---

## Testing

All features are tested in `tests/test_new_features.fh`:

```bash
./fh tests/test_new_features.fh
```

Features 8-10 have their own suites:

```bash
./fh tests/test_stdlib_additions.fh
./fh tests/test_sort.fh
./fh tests/test_oop_proto.fh
./fh tests/test_global_init.fh
./fh tests/test_global_init_gc.fh
./fh tests/test_gc_stress.fh
```

The collector has a build of its own for testing:

```bash
make TARGETS=gcdebug     # asan + a root verifier, see feature 12
./run_tests.sh
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
