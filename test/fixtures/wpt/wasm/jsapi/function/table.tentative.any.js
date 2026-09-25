// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function testfunc(n) {}

test(() => {
    var table = new WebAssembly.Table({element: "anyfunc", initial: 3})
    var func1 = new WebAssembly.Function({parameters: ["i32"], results: []}, testfunc)
    table.set(0, func1)
    var func2 = new WebAssembly.Function({parameters: ["f32"], results: []}, testfunc)
    table.set(1, func2)
    var func3 = new WebAssembly.Function({parameters: ["i64"], results: []}, testfunc)
    table.set(2, func3)

    var first = table.get(0)
    assert_true(first instanceof WebAssembly.Function)
    assert_equals(first, func1)
    assert_equals(first.type().parameters[0], func1.type().parameters[0])

    var second = table.get(1)
    assert_true(second instanceof WebAssembly.Function)
    assert_equals(second, func2)
    assert_equals(second.type().parameters[0], func2.type().parameters[0])

    var third = table.get(2)
    assert_true(third instanceof WebAssembly.Function)
    assert_equals(third, func3)
    assert_equals(third.type().parameters[0], func3.type().parameters[0])

}, "Test insertion into table")
