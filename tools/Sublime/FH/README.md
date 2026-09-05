# FH for Sublime Text

Syntax highlighting, completions, symbol navigation and a build system for the
[FH](https://github.com/MuresanVladMihail/FH) scripting language.
Works with Sublime Text 4 (build 4050+, which is required by the `version: 2`
syntax format).

## Installing

Copy the `FH` directory (the one containing this README) into your Sublime Text
`Packages` directory:

| OS      | Packages directory                                              |
| ------- | --------------------------------------------------------------- |
| Linux   | `~/.config/sublime-text/Packages/`                                |
| macOS   | `~/Library/Application Support/Sublime Text/Packages/`             |
| Windows | `%APPDATA%\Sublime Text\Packages\`                                 |

The quickest way is `Preferences > Browse Packages…` in Sublime, then drop the
folder in next to `User`:

```sh
# Linux
cp -r tools/Sublime/FH ~/.config/sublime-text/Packages/FH

# macOS
cp -r tools/Sublime/FH "$HOME/Library/Application Support/Sublime Text/Packages/FH"
```

Sublime picks the package up immediately -- no restart needed. Any `.fh` file
now opens as **FH**; otherwise pick it from `View > Syntax > FH`.

## What you get

- **`FH.sublime-syntax`** -- the grammar. It follows `src/tokenizer.c`,
  `src/parser.c` and `src/operator.c` rather than guessing, so it covers:
  - `#` line comments and `#- ... -#` block comments (non-nesting, exactly like
    the tokenizer), with `TODO`/`FIXME`/`NOTE` highlighted inside them
  - `"..."` and `'...'` strings, which may span lines, with only the escapes the
    tokenizer actually accepts (`\" \' \\ \e \n \t \r`) marked as escapes --
    anything else is flagged `invalid.illegal.unknown-escape.fh`
  - `${...}` string interpolation, highlighted as real code inside the string
  - decimal integers and floats (FH has no hex, exponent or suffix literals)
  - all 13 keywords, `true` / `false` / `null`
  - every built-in in `fh_std_c_funcs[]` plus the `len` / `append` intrinsics
  - function definitions (named and anonymous), parameters, default parameter
    values and `: "docstring"` annotations
  - the full operator set, including `?.` optional indexing, `++`/`--` and the
    bitwise operators
  - map literal keys
- **`FH.sublime-completions`** -- every built-in, plus snippets for `fn`,
  `fndoc`, `if`, `for`, `while`, `repeat`, `let`, `include` and `main`.
- **`FH.sublime-build`** -- `Tools > Build` runs the current file with `fh`.
  Error output is clickable (both `file:line:col: error: msg` forms are
  matched). Variants: run from the project root, dump bytecode (`fh -d`), dump
  documentation (`fh -o`) and run a `.fhpack` project (`fh -p`).
  If `fh` is not on your `PATH`, edit `shell_cmd` in that file.
- **`Symbol List.tmPreferences`** -- `Ctrl`/`Cmd`+`R` lists the functions in the
  file; `Goto Definition` works across a project.
- **`Comments.tmPreferences`** -- `Ctrl`/`Cmd`+`/` inserts `#`,
  `Ctrl`/`Cmd`+`Shift`+`/` wraps in `#- ... -#`.
- **`Indentation Rules.tmPreferences`** -- auto-indent after `{`, dedent on `}`.
- **`FH.sublime-settings`** -- 4-space soft tabs, no completions inside strings
  or comments.

## Running the syntax tests

`syntax_test_fh.fh` is a standard Sublime syntax-test file. With the package
installed, open it and choose `Tools > Build With… > Syntax Tests`; Sublime
reports any assertion that no longer holds. Handy after editing the grammar.

## Keeping the built-in list in sync

The built-in names in `FH.sublime-syntax` (the `builtin_*` variables) and in
`FH.sublime-completions` come from `fh_std_c_funcs[]` at the bottom of
`src/c_funcs.c`, plus `len` and `append`, which `src/compiler.c` inlines. When
you add a built-in there, add it in both places.
