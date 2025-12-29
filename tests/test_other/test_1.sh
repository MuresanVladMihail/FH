rm -rf test_1.fh && \
python generate_compilation_speed_test_fh.py > test_1.fh && \
echo "Testing parsing and compilation speed for FH" && \
time ../fh test_1.fh \
rm -rf test_1.lua && \
python generate_compilation_speed_test_lua.py > test_1.lua && \
echo "Testing parsing and compilation speed for Lua" && \
time lua test_1.lua
