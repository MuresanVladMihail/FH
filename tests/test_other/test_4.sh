echo "Testing fib 35 speed in FH" && \
time ../fh test_4.fh && \
echo "Testing fib 35 in Lua" && \
time lua test_4.lua && \
echo "Testing fib 35 in Python" && \
time python test_4.py
