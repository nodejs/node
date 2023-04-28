// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js

test(() => {
  assert_function_name(
    WebAssembly.Exception,
    "Exception",
    "WebAssembly.Exception"
  );
}, "name");

test(() => {
  assert_function_length(WebAssembly.Exception, 1, "WebAssembly.Exception");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Exception());
}, "No arguments");

test(() => {
  const tag = new WebAssembly.Tag({ parameters: [] });
  assert_throws_js(TypeError, () => WebAssembly.Exception(tag));
}, "Calling");

test(() => {
  const invalidArguments = [
    undefined,
    null,
    false,
    true,
    "",
    "test",
    Symbol(),
    1,
    NaN,
    {},
  ];
  for (const invalidArgument of invalidArguments) {
    assert_throws_js(
      TypeError,
      () => new WebAssembly.Exception(invalidArgument),
      `new Exception(${format_value(invalidArgument)})`
    );
  }
}, "Invalid descriptor argument");

test(() => {
  const typesAndArgs = [
    ["i32", 123n],
    ["i32", Symbol()],
    ["f32", 123n],
    ["f64", 123n],
    ["i64", undefined],
  ];
  for (const typeAndArg of typesAndArgs) {
    const tag = new WebAssembly.Tag({ parameters: [typeAndArg[0]] });
    assert_throws_js(
      TypeError,
      () => new WebAssembly.Exception(tag, typeAndArg[1])
    );
  }
}, "Invalid exception argument");
