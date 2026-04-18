// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function addxy(x, y) {
    return x + y
}

test(() => {
    var fun = new WebAssembly.Function({parameters: ["i32", "i32"], results: ["i32"]}, addxy);
    assert_equals(fun(1, 2), 3)
}, "test calling function")

test(() => {
    var fun = new WebAssembly.Function({parameters: ["i32", "i32"], results: ["i32"]}, addxy);
    assert_throws_js(TypeError, () => new fun(1, 2));
}, "test constructing function");
