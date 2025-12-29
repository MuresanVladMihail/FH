echo "Testing mandel benchmark speed in FH" && \
time ../fh test_3.fh && \
echo "Testing mandel benchmarkspeed in Lua" && \
time lua test_3.lua
