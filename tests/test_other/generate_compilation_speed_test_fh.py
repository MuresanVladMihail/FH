template = """

fn example_test(?)() {
    return fact_rec(?)(10) == fact_iter(?)(10);
}

#- fn f(?)() {
    let a = 2;
    let b = 42;
    let c = "hi";
} -#

fn test(?)() {
    let a(?) = ["hello"];
    let b(?) = {};
    let c(?) = "hello there";
    let d(?) = c(?) == "hello there";
    let m = math_max(1, 2);
    let mi = math_min(1, 2);
    let innerFunction = fn (a(?)) {
	let o = 42;
	let p = "42";
    };
    innerFunction(a(?));
    let innerFunction2 = fn {
	let o = 42;
	let p = "42";
    };
    innerFunction2();
}

fn Vector(?)(x, y) {
    let nx = x;
    let ny = y;
    return [nx, ny];
}

fn fact_iter(?)(n) {
    let r = 1;
    let i = 2;
    while (i < n) {
	r = r * i;
    }
    return r;
}

fn fact_rec(?)(n) {
    if (n == 0) {
        return 1;
    } else {
        return n * fact_rec(?)(n-1);
    }
}
"""

print("fn main() { return 0; }")

for i in range(1024 * 4):
    print(template.replace("(?)", str(i)))
