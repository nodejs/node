// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js

function assert_ModuleImportDescriptor(import_, expected) {
  assert_equals(Object.getPrototypeOf(import_), Object.prototype, "Prototype");
  assert_true(Object.isExtensible(import_), "isExtensible");

  const module = Object.getOwnPropertyDescriptor(import_, "module");
  assert_true(module.writable, "module: writable");
  assert_true(module.enumerable, "module: enumerable");
  assert_true(module.configurable, "module: configurable");
  assert_equals(module.value, expected.module);

  const name = Object.getOwnPropertyDescriptor(import_, "name");
  assert_true(name.writable, "name: writable");
  assert_true(name.enumerable, "name: enumerable");
  assert_true(name.configurable, "name: configurable");
  assert_equals(name.value, expected.name);

  const kind = Object.getOwnPropertyDescriptor(import_, "kind");
  assert_true(kind.writable, "kind: writable");
  assert_true(kind.enumerable, "kind: enumerable");
  assert_true(kind.configurable, "kind: configurable");
  assert_equals(kind.value, expected.kind);
}

function assert_imports(imports, expected) {
  assert_true(Array.isArray(imports), "Should be array");
  assert_equals(Object.getPrototypeOf(imports), Array.prototype, "Prototype");
  assert_true(Object.isExtensible(imports), "isExtensible");

  assert_equals(imports.length, expected.length);
  for (let i = 0; i < expected.length; ++i) {
    assert_ModuleImportDescriptor(imports[i], expected[i]);
  }
}

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  assert_throws_js(TypeError, () => WebAssembly.Module.imports());
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
    assert_throws_js(TypeError, () => WebAssembly.Module.imports(argument),
                     `imports(${format_value(argument)})`);
  }
}, "Non-Module arguments");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const fn = WebAssembly.Module.imports;
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
    assert_array_equals(fn.call(thisValue, module), []);
  }
}, "Branding");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const imports = WebAssembly.Module.imports(module);
  assert_true(Array.isArray(imports));
}, "Return type");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const imports = WebAssembly.Module.imports(module);
  assert_imports(imports, []);
}, "Empty module");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  assert_not_equals(WebAssembly.Module.imports(module), WebAssembly.Module.imports(module));
}, "Empty module: array caching");

test(() => {
  const builder = new WasmModuleBuilder();

  builder.addImport("module", "fn", kSig_v_v);
  builder.addImportedGlobal("module", "global", kWasmI32);
  builder.addImportedMemory("module", "memory", 0, 128);
  builder.addImportedTable("module", "table", 0, 128);

  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);
  const imports = WebAssembly.Module.imports(module);
  const expected = [
    { "module": "module", "kind": "function", "name": "fn" },
    { "module": "module", "kind": "global", "name": "global" },
    { "module": "module", "kind": "memory", "name": "memory" },
    { "module": "module", "kind": "table", "name": "table" },
  ];
  assert_imports(imports, expected);
}, "imports");

test(() => {
  const builder = new WasmModuleBuilder();

  builder.addImport("", "fn", kSig_v_v);
  builder.addImportedGlobal("", "global", kWasmI32);
  builder.addImportedMemory("", "memory", 0, 128);
  builder.addImportedTable("", "table", 0, 128);

  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);
  const imports = WebAssembly.Module.imports(module);
  const expected = [
    { "module": "", "kind": "function", "name": "fn" },
    { "module": "", "kind": "global", "name": "global" },
    { "module": "", "kind": "memory", "name": "memory" },
    { "module": "", "kind": "table", "name": "table" },
  ];
  assert_imports(imports, expected);
}, "imports with empty module name");

test(() => {
  const builder = new WasmModuleBuilder();

  builder.addImport("a", "", kSig_v_v);
  builder.addImportedGlobal("b", "", kWasmI32);
  builder.addImportedMemory("c", "", 0, 128);
  builder.addImportedTable("d", "", 0, 128);

  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);
  const imports = WebAssembly.Module.imports(module);
  const expected = [
    { "module": "a", "kind": "function", "name": "" },
    { "module": "b", "kind": "global", "name": "" },
    { "module": "c", "kind": "memory", "name": "" },
    { "module": "d", "kind": "table", "name": "" },
  ];
  assert_imports(imports, expected);
}, "imports with empty names");

test(() => {
  const builder = new WasmModuleBuilder();

  builder.addImport("", "", kSig_v_v);
  builder.addImportedGlobal("", "", kWasmI32);
  builder.addImportedMemory("", "", 0, 128);
  builder.addImportedTable("", "", 0, 128);

  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);
  const imports = WebAssembly.Module.imports(module);
  const expected = [
    { "module": "", "kind": "function", "name": "" },
    { "module": "", "kind": "global", "name": "" },
    { "module": "", "kind": "memory", "name": "" },
    { "module": "", "kind": "table", "name": "" },
  ];
  assert_imports(imports, expected);
}, "imports with empty module names and names");

test(() => {
  const module = new WebAssembly.Module(emptyModuleBinary);
  const imports = WebAssembly.Module.imports(module, {});
  assert_imports(imports, []);
}, "Stray argument");
