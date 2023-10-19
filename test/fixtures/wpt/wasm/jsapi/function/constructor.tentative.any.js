// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js

function addxy(x, y) {
    return x + y
}

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_function_name(WebAssembly.Function, "Function", "WebAssembly.Function");
}, "name");

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_function_length(WebAssembly.Function, 2, "WebAssembly.Function");
}, "length");

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function());
    const argument = {parameters: [], results: []};
    assert_throws_js(TypeError, () => new WebAssembly.Function(argument));
}, "Too few arguments");

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    const arguments = [{parameters: ["i32", "i32"], results: ["i32"]}, addxy];
    assert_throws_js(TypeError, () => WebAssembly.Function(...arguments));
}, "Calling");

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    var fun = new WebAssembly.Function({parameters: ["i32", "i32"], results: ["i32"]}, addxy);
    assert_true(fun instanceof WebAssembly.Function)
}, "construct with JS function")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({parameters: []}, addxy))
}, "fail with missing results")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({results: []}, addxy))
}, "fail with missing parameters")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({parameters: [1], results: [true]}, addxy))
}, "fail with non-string parameters & results")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({parameters: ["invalid"], results: ["invalid"]}, addxy))
}, "fail with non-existent parameter and result type")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({parameters: [], results: []}, 72))
}, "fail with non-function object")

test(() => {
    assert_implements(WebAssembly.Function, "WebAssembly.Function is not implemented");
    assert_throws_js(TypeError, () => new WebAssembly.Function({parameters: [], results: []}, {}))
}, "fail to construct with non-callable object")
