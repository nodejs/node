// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

test(() => {
  assert_function_name(WebAssembly.Tag, "Tag", "WebAssembly.Tag");
}, "name");

test(() => {
  assert_function_length(WebAssembly.Tag, 1, "WebAssembly.Tag");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Tag());
}, "No arguments");

test(() => {
  const argument = { parameters: [] };
  assert_throws_js(TypeError, () => WebAssembly.Tag(argument));
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
      () => new WebAssembly.Tag(invalidArgument),
      `new Tag(${format_value(invalidArgument)})`
    );
  }
}, "Invalid descriptor argument");

test(() => {
  const invalidTypes = ["i16", "i128", "f16", "f128", "u32", "u64", "i32\0"];
  for (const value of invalidTypes) {
    const argument = { parameters: [value] };
    assert_throws_js(TypeError, () => new WebAssembly.Tag(argument));
  }
}, "Invalid type parameter");
