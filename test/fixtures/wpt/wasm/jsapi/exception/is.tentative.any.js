// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/memory/assertions.js

test(() => {
  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  assert_throws_js(TypeError, () => exn.is());
}, "Missing arguments");

test(() => {
  const invalidValues = [undefined, null, true, "", Symbol(), 1, {}];
  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  for (argument of invalidValues) {
    assert_throws_js(TypeError, () => exn.is(argument));
  }
}, "Invalid exception argument");

test(() => {
  const tag1 = new WebAssembly.Tag({ parameters: ["i32"] });
  const tag2 = new WebAssembly.Tag({ parameters: ["i32"] });
  const exn = new WebAssembly.Exception(tag1, [42]);
  assert_true(exn.is(tag1));
  assert_false(exn.is(tag2));
}, "is");
