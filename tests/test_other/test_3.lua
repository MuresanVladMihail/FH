function calc_point(cx, cy, max_iter) 
    local i = 0;
    local x = 0;
    local y = 0;
    while (i < max_iter) do
        local t = x*x - y*y + cx;
        y = 2*x*y + cy;
        x = t;
        if (x*x + y*y > 4) then
            break;
        end
        i = i + 1;
    end
    return i;
end

function mandelbrot(x1,y1, x2,y2, size_x,size_y, max_iter) 
    local step_x = (x2-x1)/(size_x-1);
    local step_y = (y2-y1)/(size_y-1);

    local y = y1;
    while (y <= y2) do
        local x = x1;
        while (x <= x2) do
            local c = calc_point(x, y, max_iter);
            if (c == max_iter) then 
            else
            end
            x = x + step_x;
        end
        y = y + step_y;
    end
end

function main() 
  mandelbrot(-2, -2, 2, 2, 500, 500, 1500);
end

main();
