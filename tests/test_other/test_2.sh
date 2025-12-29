echo "Testing function call speed in FH" && \
time ../fh test_2.fh && \
echo "Testing function call speed in Lua" && \
time lua test_2.lua
