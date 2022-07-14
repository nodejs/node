// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js

function assert_type(argument) {
    const myglobal = new WebAssembly.Global(argument);
    const globaltype = myglobal.type();

    assert_equals(globaltype.value, argument.value);
    assert_equals(globaltype.mutable, argument.mutable);
}

test(() => {
    assert_type({ "value": "i32", "mutable": true});
}, "i32, mutable");

test(() => {
    assert_type({ "value": "i32", "mutable": false});
}, "i32, immutable");

test(() => {
    assert_type({ "value": "i64", "mutable": true});
}, "i64, mutable");

test(() => {
    assert_type({ "value": "i64", "mutable": false});
}, "i64, immutable");

test(() => {
    assert_type({ "value": "f32", "mutable": true});
}, "f32, mutable");

test(() => {
    assert_type({ "value": "f32", "mutable": false});
}, "f32, immutable");

test(() => {
    assert_type({ "value": "f64", "mutable": true});
}, "f64, mutable");

test(() => {
    assert_type({ "value": "f64", "mutable": false});
}, "f64, immutable");

test(() => {
    assert_type({"value": "externref", "mutable": true})
}, "externref, mutable")

test(() => {
    assert_type({"value": "externref", "mutable": false})
}, "externref, immutable")

test(() => {
    assert_type({"value": "anyfunc", "mutable": true})
}, "anyfunc, mutable")

test(() => {
    assert_type({"value": "anyfunc", "mutable": false})
}, "anyfunc, immutable")

test(() => {
    const myglobal = new WebAssembly.Global({"value": "i32", "mutable": true});
    const propertyNames = Object.getOwnPropertyNames(myglobal.type());
    assert_equals(propertyNames[0], "mutable");
    assert_equals(propertyNames[1], "value");
}, "key ordering");
