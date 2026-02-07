# Stacktrace Improvements

## Summary

Significantly improved error reporting with detailed, readable stacktraces that show the complete call chain when errors occur.

## Changes Made

### Before
```
CALLSTACK BEGIN:
 test_error.fh - level3:2:4
 test_error.fh - level2:8:4
 test_error.fh - level1:12:4
 test_error.fh - main:16:4
CALLSTACK END
ERROR: test_error.fh:4:13: division by zero
last called from: test_error.fh:18:5
```

Issues with old format:
- Shows function **definition** locations instead of **call sites**
- Poor formatting and hard to read
- Confusing "last called from" message
- Doesn't follow standard stacktrace conventions

### After
```
ERROR: test_error.fh:5:14: error: division by zero

Traceback (most recent call last):
  File "test_error.fh", line 17, in main
  File "test_error.fh", line 13, in level1
  File "test_error.fh", line 9, in level2
  File "test_error.fh", line 5, in level3
```

Improvements:
- **Shows actual call sites**: Line numbers indicate where each function was called FROM
- **Clear formatting**: Follows Python/Node.js stacktrace conventions
- **Error first**: Error message appears at the top, followed by traceback
- **Proper ordering**: Bottom-up (oldest to newest) call chain
- **Better context**: Shows which function is executing and where it was called

## Technical Details

### Implementation (src/program.c)

1. **Rewrote `fh_get_error()` function**:
   - Uses `frame->ret_addr` to find actual call sites
   - Converts bytecode addresses to source locations using `fh_get_addr_src_loc()`
   - Formats output in standard "File, line, in function" format
   - Handles both VM function frames and C function frames

2. **Key insight**:
   - Each call frame stores `ret_addr` (return address) pointing to the instruction after the call
   - By looking at frame[N]'s ret_addr and converting it using frame[N-1]'s func_def, we find where frame[N] was called from
   - For the topmost frame (where error occurred), use the error location directly

3. **Buffer management**:
   - Increased buffer size to 2048 bytes to accommodate full stacktraces
   - Prevents overflow with length checks during string building

## Testing

Created demonstration files:
- `examples/stacktrace_demo.fh` - Shows stacktrace with multi-level function calls
- Verified all 38 existing tests still pass
- Tested with various error types (division by zero, array bounds, arithmetic errors)

## Benefits

1. **Better debugging**: Developers can immediately see the call chain leading to errors
2. **Familiar format**: Matches conventions from Python, JavaScript, and other languages
3. **More information**: Shows both where errors occur AND how the code got there
4. **Professional quality**: Brings FH's error reporting up to modern language standards

## Files Modified

- `src/program.c`: Rewrote `fh_get_error()` function (lines 129-194)
- `README.md`: Added Error Reporting section documenting the feature
- `examples/stacktrace_demo.fh`: Demo file showing the improved stacktraces
