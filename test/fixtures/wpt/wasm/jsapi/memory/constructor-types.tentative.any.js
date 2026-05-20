// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/memory/assertions.js

test(() => {
    const argument = { initial: 5, minimum: 6 };
    assert_throws_js(TypeError, () => new WebAssembly.Memory(argument));
}, "Initializing with both initial and minimum");

test(() => {
    const argument = { minimum: 0 };
    const memory = new WebAssembly.Memory(argument);
    assert_Memory(memory, { "size": 0 });
  }, "Zero minimum");

test(() => {
    const argument = { minimum: 4 };
    const memory = new WebAssembly.Memory(argument);
    assert_Memory(memory, { "size": 4 });
  }, "Non-zero minimum");
