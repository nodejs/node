// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function assert_type(argument) {
    const memory = new WebAssembly.Memory(argument);
    const memorytype = memory.type()

    assert_equals(memorytype.minimum, argument.minimum);
    assert_equals(memorytype.maximum, argument.maximum);
    if (argument.shared !== undefined) {
        assert_equals(memorytype.shared, argument.shared);
    }
}

test(() => {
    assert_type({ "minimum": 0 });
}, "Zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 5 });
}, "Non-zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 0 });
}, "Zero maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 5 });
}, "None-zero maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 10,  "shared": false});
}, "non-shared memory");

test(() => {
    assert_type({ "minimum": 0, "maximum": 10, "shared": true});
}, "shared memory");
