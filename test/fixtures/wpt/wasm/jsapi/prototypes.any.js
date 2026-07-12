// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/wasm-module-builder.js

let emptyModuleBinary;
setup(() => {
  emptyModuleBinary = new WasmModuleBuilder().toBuffer();
});

test(() => {
  class _Module extends WebAssembly.Module {}
  let module = new _Module(emptyModuleBinary);
  assert_true(module instanceof _Module, "_Module instanceof _Module");
  assert_true(module instanceof WebAssembly.Module, "_Module instanceof WebAssembly.Module");
}, "_Module");

test(() => {
  class _Instance extends WebAssembly.Instance {}
  let instance = new _Instance(new WebAssembly.Module(emptyModuleBinary));
  assert_true(instance instanceof _Instance, "_Instance instanceof _Instance");
  assert_true(instance instanceof WebAssembly.Instance, "_Instance instanceof WebAssembly.Instance");
}, "_Instance");

test(() => {
  class _Memory extends WebAssembly.Memory {}
  let memory = new _Memory({initial: 0, maximum: 1});
  assert_true(memory instanceof _Memory, "_Memory instanceof _Memory");
  assert_true(memory instanceof WebAssembly.Memory, "_Memory instanceof WebAssembly.Memory");
}, "_Memory");

test(() => {
  class _Table extends WebAssembly.Table {}
  let table = new _Table({initial: 0, element: "anyfunc"});
  assert_true(table instanceof _Table, "_Table instanceof _Table");
  assert_true(table instanceof WebAssembly.Table, "_Table instanceof WebAssembly.Table");
}, "_Table");

test(() => {
  class _Global extends WebAssembly.Global {}
  let global = new _Global({value: "i32", mutable: false}, 0);
  assert_true(global instanceof _Global, "_Global instanceof _Global");
  assert_true(global instanceof WebAssembly.Global, "_Global instanceof WebAssembly.Global");
}, "_Global");
