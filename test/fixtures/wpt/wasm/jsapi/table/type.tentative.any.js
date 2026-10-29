// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function assert_type(argument) {
    const mytable = new WebAssembly.Table(argument);
    const tabletype = mytable.type()
    assert_equals(tabletype.minimum, argument.minimum);
    assert_equals(tabletype.maximum, argument.maximum);
    assert_equals(tabletype.element, argument.element);
}

test(() => {
    assert_type({ "minimum": 0, "element": "funcref"});
}, "Zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 5, "element": "funcref" });
}, "Non-zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 0, "element": "funcref" });
}, "Zero maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 5, "element": "funcref" });
}, "Non-zero maximum");
