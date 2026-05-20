// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function assert_type(argument) {
  const tag = new WebAssembly.Tag(argument);
  const tagtype = tag.type();

  assert_array_equals(tagtype.parameters, argument.parameters);
}

test(() => {
  assert_type({ parameters: [] });
}, "[]");

test(() => {
  assert_type({ parameters: ["i32", "i64"] });
}, "[i32 i64]");

test(() => {
  assert_type({ parameters: ["i32", "i64", "f32", "f64"] });
}, "[i32 i64 f32 f64]");
