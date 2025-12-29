
-- function call speed benchmark

function f(x)
  return x;
end

function main()

  local function f2(x) 
    return x;
  end

  local i = 0;
  while (i < 200000000) do 
    f(i);
    f2(i);
    i=i+1;
   end
   print(i);
end
main();
