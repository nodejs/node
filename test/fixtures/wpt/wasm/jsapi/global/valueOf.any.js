// META: global=window,dedicatedworker,jsshell,shadowrealm

test(() => {
  const argument = { "value": "i32" };
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Global,
    WebAssembly.Global.prototype,
  ];

  const fn = WebAssembly.Global.prototype.valueOf;

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fn.call(thisValue), `this=${format_value(thisValue)}`);
  }
}, "Branding");

test(() => {
  const argument = { "value": "i32" };
  const global = new WebAssembly.Global(argument, 0);
  assert_equals(global.valueOf({}), 0);
}, "Stray argument");
