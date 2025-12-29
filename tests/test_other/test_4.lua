function fib(n) 
    if (n >= 2) then
        return fib (n - 1) + fib (n - 2);
    else 
        return n;
    end
end

function main() 
    print(fib(35));
end
main();
