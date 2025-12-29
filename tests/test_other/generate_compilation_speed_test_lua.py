template = """

function example_test(?)()
    return fact_rec(?)(10) == fact_iter(?)(10);
end

--[[ function f(?)() {
      local a = 2;
      local b = 42;
      local c = "hi";
} --]]

function test(?)()
    local a(?) = {"hello"};
    local b(?) = {};
    local c(?) = "hello there";
    local d(?) = c(?) == "hello there";
    local m = math.max(1, 2);
    local mi = math.min(1, 2);
    local function innerFunction(a(?))
        local o = 42;
        local p = "42";
    end
    innerFunction(a(?));
    local function innerFunction2()
        local o = 42;
        local p = "42";
    end
    innerFunction2();
end

function Vector(?)(x, y)
    local nx = x;
    local ny = y;
    return {nx, ny};
end

function fact_iter(?)(n)
    local r = 1;
    local i = 2;
    while (i < n) do
        r = r * i;
    end
    return r;
end

function fact_rec(?)(n)
    if (n == 0) then
        return 1;
    else
        return n * fact_rec(?)(n-1);
    end
end
"""

print("function main() return 0; end")

for i in range(1024 * 4):
    print(template.replace("(?)", str(i)))
