// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js

function assert_ArrayBuffer(buffer, expected) {
  assert_equals(Object.getPrototypeOf(buffer), ArrayBuffer.prototype, "Prototype");
  assert_true(Object.isExtensible(buffer), "isExtensible");
  assert_array_equals(new Uint8Array(buffer), expected);
}

function assert_sections(sections, expected) {
  assert_true(Array.isArray(sections), "Should be array");
  assert_equals(Object.getPrototypeOf(sections), Array.prototype, "Prototype");
  assert_true(Object.isExtensible(sections), "isExtensible");

  assert_equals(sections.length, expected.length);
  for (let i = 0; i < expected.length; ++i) {
    assert_ArrayBuffer(sections[i], expected[i]);
  }
}

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  assert_throws_js(TypeError, () => WebAssembly.Module.customSections());
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_throws_js(TypeError, () => WebAssembly.Module.customSections(module));
}, "Missing arguments");

test(() => {
  const invalidArguments = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Module,
    WebAssembly.Module.prototype,
  ];
  for (const argument of invalidArguments) {
    assert_throws_js(TypeError, () => WebAssembly.Module.customSections(argument, ""),
                     `customSections(${format_value(argument)})`);
  }
}, "Non-Module arguments");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const fn = WebAssembly.Module.customSections;
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Module,
    WebAssembly.Module.prototype,
  ];
  for (const thisValue of thisValues) {
    assert_sections(fn.call(thisValue, module, ""), []);
  }
}, "Branding");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_sections(WebAssembly.Module.customSections(module, ""), []);
}, "Empty module");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_not_equals(WebAssembly.Module.customSections(module, ""),
                    WebAssembly.Module.customSections(module, ""));
}, "Empty module: array caching");

test(() => {
  const bytes1 = [87, 101, 98, 65, 115, 115, 101, 109, 98, 108, 121];
  const bytes2 = [74, 83, 65, 80, 73];

  const builder = new WasmModuleBuilder();
  builder.addCustomSection("name", bytes1);
  builder.addCustomSection("name", bytes2);
  builder.addCustomSection("foo", bytes1);
  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);

  assert_sections(WebAssembly.Module.customSections(module, "name"), [
    bytes1,
    bytes2,
  ])

  assert_sections(WebAssembly.Module.customSections(module, "foo"), [
    bytes1,
  ])

  assert_sections(WebAssembly.Module.customSections(module, ""), [])
  assert_sections(WebAssembly.Module.customSections(module, "\0"), [])
  assert_sections(WebAssembly.Module.customSections(module, "name\0"), [])
  assert_sections(WebAssembly.Module.customSections(module, "foo\0"), [])
}, "Custom sections");

test(() => {
  const bytes = [87, 101, 98, 65, 115, 115, 101, 109, 98, 108, 121];
  const name = "yee\uD801\uDC37eey"

  const builder = new WasmModuleBuilder();
  builder.addCustomSection(name, bytes);
  const buffer = builder.toBuffer();
  const module = new WebAssembly.Module(buffer);

  assert_sections(WebAssembly.Module.customSections(module, name), [
    bytes,
  ]);
  assert_sections(WebAssembly.Module.customSections(module, "yee\uFFFDeey"), []);
  assert_sections(WebAssembly.Module.customSections(module, "yee\uFFFD\uFFFDeey"), []);
}, "Custom sections with surrogate pairs");

test(() => {
  const bytes = [87, 101, 98, 65, 115, 115, 101, 109, 98, 108, 121];

  const builder = new WasmModuleBuilder();
  builder.addCustomSection("na\uFFFDme", bytes);
  const buffer = builder.toBuffer();
  const module = new WebAssembly.Module(buffer);

  assert_sections(WebAssembly.Module.customSections(module, "name"), []);
  assert_sections(WebAssembly.Module.customSections(module, "na\uFFFDme"), [
    bytes,
  ]);
  assert_sections(WebAssembly.Module.customSections(module, "na\uDC01me"), []);
}, "Custom sections with U+FFFD");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_sections(WebAssembly.Module.customSections(module, "", {}), []);
}, "Stray argument");
