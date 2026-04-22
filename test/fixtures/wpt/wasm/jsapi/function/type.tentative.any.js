// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function addNumbers(x, y, z) {
    return x+y+z;
}

function doNothing() {}

function assert_function(functype, func) {
    var wasmFunc = new WebAssembly.Function(functype, func);
    assert_equals(functype.parameters.length, wasmFunc.type().parameters.length);
    for(let i = 0; i < functype.parameters.length; i++) {
        assert_equals(functype.parameters[i], wasmFunc.type().parameters[i]);
    }
    assert_equals(functype.results.length, wasmFunc.type().results.length);
    for(let i = 0; i < functype.results.length; i++) {
        assert_equals(functype.results[i], wasmFunc.type().results[i]);
    }
}

test(() => {
    assert_function({results: [], parameters: []}, doNothing);
}, "Check empty results and parameters")

test(() => {
    assert_function({results: ["f64"], parameters: ["i32", "i64", "f32"]}, addNumbers)
}, "Check all types")
