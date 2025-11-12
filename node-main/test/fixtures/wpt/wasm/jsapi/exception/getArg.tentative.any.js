// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/memory/assertions.js

test(() => {
  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  assert_throws_js(TypeError, () => exn.getArg());
  assert_throws_js(TypeError, () => exn.getArg(tag));
}, "Missing arguments");

test(() => {
  const invalidValues = [undefined, null, true, "", Symbol(), 1, {}];
  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  for (argument of invalidValues) {
    assert_throws_js(TypeError, () => exn.getArg(argument, 0));
  }
}, "Invalid exception argument");

test(() => {
  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  assert_throws_js(RangeError, () => exn.getArg(tag, 1));
}, "Index out of bounds");

test(() => {
  const outOfRangeValues = [
    undefined,
    NaN,
    Infinity,
    -Infinity,
    -1,
    0x100000000,
    0x1000000000,
    "0x100000000",
    {
      valueOf() {
        return 0x100000000;
      },
    },
  ];

  const tag = new WebAssembly.Tag({ parameters: [] });
  const exn = new WebAssembly.Exception(tag, []);
  for (const value of outOfRangeValues) {
    assert_throws_js(RangeError, () => exn.getArg(tag, value));
  }
}, "Getting out-of-range argument");

test(() => {
  const tag = new WebAssembly.Tag({ parameters: ["i32"] });
  const exn = new WebAssembly.Exception(tag, [42]);
  assert_equals(exn.getArg(tag, 0), 42);
}, "getArg");
