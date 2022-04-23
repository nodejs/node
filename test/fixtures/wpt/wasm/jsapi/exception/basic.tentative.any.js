// META: global=window,worker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

function assert_throws_wasm(fn, message) {
  try {
    fn();
    assert_not_reached(`expected to throw with ${message}`);
  } catch (e) {
    assert_true(e instanceof WebAssembly.Exception, `Error should be a WebAssembly.Exception with ${message}`);
  }
}

promise_test(async () => {
  const kWasmAnyRef = 0x6f;
  const kSig_v_r = makeSig([kWasmAnyRef], []);
  const builder = new WasmModuleBuilder();
  const except = builder.addException(kSig_v_r);
  builder.addFunction("throw_param", kSig_v_r)
    .addBody([
      kExprLocalGet, 0,
      kExprThrow, except,
    ])
    .exportFunc();
  const buffer = builder.toBuffer();
  const {instance} = await WebAssembly.instantiate(buffer, {});
  const values = [
    undefined,
    null,
    true,
    false,
    "test",
    Symbol(),
    0,
    1,
    4.2,
    NaN,
    Infinity,
    {},
    () => {},
  ];
  for (const v of values) {
    assert_throws_wasm(() => instance.exports.throw_param(v), String(v));
  }
}, "Wasm function throws argument");

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  const except = builder.addException(kSig_v_a);
  builder.addFunction("throw_null", kSig_v_v)
    .addBody([
      kExprRefNull, kWasmAnyFunc,
      kExprThrow, except,
    ])
    .exportFunc();
  const buffer = builder.toBuffer();
  const {instance} = await WebAssembly.instantiate(buffer, {});
  assert_throws_wasm(() => instance.exports.throw_null());
}, "Wasm function throws null");

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  const except = builder.addException(kSig_v_i);
  builder.addFunction("throw_int", kSig_v_v)
    .addBody([
      ...wasmI32Const(7),
      kExprThrow, except,
    ])
    .exportFunc();
  const buffer = builder.toBuffer();
  const {instance} = await WebAssembly.instantiate(buffer, {});
  assert_throws_wasm(() => instance.exports.throw_int());
}, "Wasm function throws integer");

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  const fnIndex = builder.addImport("module", "fn", kSig_v_v);
  const except = builder.addException(kSig_v_r);
  builder.addFunction("catch_exception", kSig_r_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, fnIndex,
      kExprCatch, except,
        kExprReturn,
      kExprEnd,
      kExprRefNull, kWasmAnyRef,
    ])
    .exportFunc();

  const buffer = builder.toBuffer();

  const error = new Error();
  const fn = () => { throw error };
  const {instance} = await WebAssembly.instantiate(buffer, {
    module: { fn }
  });
  assert_throws_exactly(error, () => instance.exports.catch_exception());
}, "Imported JS function throws");

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  const fnIndex = builder.addImport("module", "fn", kSig_v_v);
  builder.addFunction("catch_and_rethrow", kSig_r_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, fnIndex,
      kExprCatchAll,
        kExprRethrow, 0x00,
      kExprEnd,
      kExprRefNull, kWasmAnyRef,
    ])
    .exportFunc();

  const buffer = builder.toBuffer();

  const error = new Error();
  const fn = () => { throw error };
  const {instance} = await WebAssembly.instantiate(buffer, {
    module: { fn }
  });
  assert_throws_exactly(error, () => instance.exports.catch_and_rethrow());
}, "Imported JS function throws, Wasm catches and rethrows");
