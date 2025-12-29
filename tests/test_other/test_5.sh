echo "Testing creation of 1 million maps in FH" && \
time ../fh test_5.fh && \
echo "Testing creation of 1 million maps in Lua" && \
time lua test_5.lua
