// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const tag = new WebAssembly.Tag({ parameters: ["i32"] });
  const exn = new WebAssembly.Exception(tag, [42]);
  const exn_same_payload = new WebAssembly.Exception(tag, [42]);
  const exn_diff_payload = new WebAssembly.Exception(tag, [53]);

  const builder = new WasmModuleBuilder();
  const jsfuncIndex = builder.addImport("module", "jsfunc", kSig_v_v);
  const tagIndex = builder.addImportedTag("module", "tag", kSig_v_i);
  const imports = {
    module: {
      jsfunc: function() { throw exn; },
      tag: tag
    }
  };

  builder
    .addFunction("catch_rethrow", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, jsfuncIndex,
      kExprCatch, tagIndex,
        kExprDrop,
        kExprRethrow, 0x00,
      kExprEnd
    ])
    .exportFunc();

  builder
    .addFunction("catch_all_rethrow", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, jsfuncIndex,
      kExprCatchAll,
        kExprRethrow, 0x00,
      kExprEnd
    ])
    .exportFunc();

  const buffer = builder.toBuffer();
  WebAssembly.instantiate(buffer, imports).then(result => {
    try {
      result.instance.exports.catch_rethrow();
    } catch (e) {
      assert_equals(e, exn);
      assert_not_equals(e, exn_same_payload);
      assert_not_equals(e, exn_diff_payload);
    }
    try {
      result.instance.exports.catch_all_rethrow();
    } catch (e) {
      assert_equals(e, exn);
      assert_not_equals(e, exn_same_payload);
      assert_not_equals(e, exn_diff_payload);
    }
  });
}, "Identity check");
