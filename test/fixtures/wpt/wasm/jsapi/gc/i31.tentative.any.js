// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

let exports = {};
setup(() => {
  const builder = new WasmModuleBuilder();
  const i31Ref = wasmRefType(kWasmI31Ref);
  const i31NullableRef = wasmRefNullType(kWasmI31Ref);
  const anyRef = wasmRefType(kWasmAnyRef);

  builder
    .addFunction("makeI31", makeSig_r_x(i31Ref, kWasmI32))
    .addBody([kExprLocalGet, 0,
              ...GCInstr(kExprI31New)])
    .exportFunc();

  builder
    .addFunction("castI31", makeSig_r_x(kWasmI32, anyRef))
    .addBody([kExprLocalGet, 0,
              ...GCInstr(kExprRefCast), kI31RefCode,
              ...GCInstr(kExprI31GetU)])
    .exportFunc();

  builder
    .addFunction("getI31", makeSig_r_x(kWasmI32, i31Ref))
    .addBody([kExprLocalGet, 0,
              ...GCInstr(kExprI31GetS)])
    .exportFunc();

  builder
    .addFunction("argI31", makeSig_v_x(i31NullableRef))
    .addBody([])
    .exportFunc();

  builder
    .addGlobal(i31NullableRef, true, [...wasmI32Const(0), ...GCInstr(kExprI31New)])
  builder
    .addExportOfKind("i31Global", kExternalGlobal, 0);

  builder
    .addTable(i31NullableRef, 10)
  builder
    .addExportOfKind("i31Table", kExternalTable, 0);

  const buffer = builder.toBuffer();
  const module = new WebAssembly.Module(buffer);
  const instance = new WebAssembly.Instance(module, {});
  exports = instance.exports;
});

test(() => {
  assert_equals(exports.makeI31(42), 42);
  assert_equals(exports.makeI31(2 ** 30 - 1), 2 ** 30 - 1);
  assert_equals(exports.makeI31(2 ** 30), -(2 ** 30));
  assert_equals(exports.makeI31(-(2 ** 30)), -(2 ** 30));
  assert_equals(exports.makeI31(2 ** 31 - 1), -1);
  assert_equals(exports.makeI31(2 ** 31), 0);
}, "i31ref conversion to Number");

test(() => {
  assert_equals(exports.getI31(exports.makeI31(42)), 42);
  assert_equals(exports.getI31(42), 42);
  assert_equals(exports.getI31(2.0 ** 30 - 1), 2 ** 30 - 1);
  assert_equals(exports.getI31(-(2 ** 30)), -(2 ** 30));
}, "Number conversion to i31ref");

test(() => {
  exports.argI31(null);
  assert_throws_js(TypeError, () => exports.argI31(2 ** 30));
  assert_throws_js(TypeError, () => exports.argI31(-(2 ** 30) - 1));
  assert_throws_js(TypeError, () => exports.argI31(2n));
  assert_throws_js(TypeError, () => exports.argI31(() => 3));
  assert_throws_js(TypeError, () => exports.argI31(exports.getI31));
}, "Check i31ref argument type");

test(() => {
  assert_equals(exports.castI31(42), 42);
  assert_equals(exports.castI31(2 ** 30 - 1), 2 ** 30 - 1);
  assert_throws_js(WebAssembly.RuntimeError, () => { exports.castI31(2 ** 30); });
  assert_throws_js(WebAssembly.RuntimeError, () => { exports.castI31(-(2 ** 30) - 1); });
  assert_throws_js(WebAssembly.RuntimeError, () => { exports.castI31(2 ** 32); });
}, "Numbers in i31 range are i31ref, not hostref");

test(() => {
  assert_equals(exports.i31Global.value, 0);
  exports.i31Global.value = 42;
  assert_throws_js(TypeError, () => exports.i31Global.value = 2 ** 30);
  assert_throws_js(TypeError, () => exports.i31Global.value = -(2 ** 30) - 1);
  assert_equals(exports.i31Global.value, 42);
}, "i31ref global");

test(() => {
  assert_equals(exports.i31Table.get(0), null);
  exports.i31Table.set(0, 42);
  assert_throws_js(TypeError, () => exports.i31Table.set(0, 2 ** 30));
  assert_throws_js(TypeError, () => exports.i31Table.set(0, -(2 ** 30) - 1));
  assert_equals(exports.i31Table.get(0), 42);
}, "i31ref table");
