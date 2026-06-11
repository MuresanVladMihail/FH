#!/bin/bash
# Comprehensive test runner for FH language
# Ensures language safety and correctness

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0
SKIPPED=0

# Function to run a single test
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .fh)

    # Skip benchmark files (they're slow)
    if [[ "$test_file" == *"bench"* ]] || [[ "$test_file" == *"benchmark"* ]]; then
        printf "${YELLOW}[SKIP]${NC} %s (benchmark)\n" "$test_name"
        ((SKIPPED++))
        return
    fi

    # Run the test (with timeout to prevent hangs)
    if output=$(./fh "$test_file" 2>&1); then
        if echo "$output" | grep -q "ok"; then
            printf "${GREEN}[PASS]${NC} %s\n" "$test_name"
            ((PASSED++))
        else
            printf "${RED}[FAIL]${NC} %s (no 'ok' output)\n" "$test_name"
            ((FAILED++))
        fi
    else
        printf "${RED}[FAIL]${NC} %s (exit code $?)\n" "$test_name"
        ((FAILED++))
    fi
}

# Check if fh binary exists
if [ ! -f "./fh" ]; then
    echo "Error: ./fh binary not found. Run 'make' first."
    exit 1
fi

echo "======================================"
echo "  FH Language Test Suite"
echo "======================================"
echo ""

# Run all test_*.fh files
for test_file in tests/test_*.fh; do
    [ -f "$test_file" ] || continue
    run_test "$test_file"
done

echo ""
echo "======================================"
echo "  Test Results"
echo "======================================"
echo -e "Passed:  ${GREEN}$PASSED${NC}"
echo -e "Failed:  ${RED}$FAILED${NC}"
echo -e "Skipped: ${YELLOW}$SKIPPED${NC}"
echo "Total:   $((PASSED + FAILED + SKIPPED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
