// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/jspi/testharness-additions.js

test(() => {
    let builder = new WasmModuleBuilder();
    let js_tag = builder.addImportedTag("", "tag", kSig_v_r);
    let try_sig_index = builder.addType(kSig_i_v);

    let promise42 = new WebAssembly.Suspending(() => Promise.resolve(42));
    let kPromise42Ref = builder.addImport("", "promise42", kSig_i_v);

    builder.addFunction("test", kSig_i_v)
        .addBody([
        kExprTry, try_sig_index,
            kExprCallFunction, kPromise42Ref,
            kExprReturn,  // If there was no trap or exception, return
        kExprCatch, js_tag,
            kExprI32Const, 43,
            kExprReturn,
        kExprEnd,
        ])
        .exportFunc();

    let instance = builder.instantiate({"": {
        promise42: promise42,
        tag: WebAssembly.JSTag,
    }});

    assert_equals(43, instance.exports.test());
  },"catch SuspendError");

promise_test(async t=>{
    let builder = new WasmModuleBuilder();
    let js_tag = builder.addImportedTag("", "tag", kSig_v_r);
    let try_sig_index = builder.addType(kSig_i_v);

    let promise42 = new WebAssembly.Suspending(() => Promise.resolve(42));
    let kPromise42Ref = builder.addImport("", "promise42", kSig_i_v);
    let backChnnlRef = builder.addImport("","backChnnl",kSig_i_v);

    builder.addFunction("main", kSig_i_v)
      .addBody([
        kExprTry, try_sig_index,
          kExprCallFunction, backChnnlRef,
          kExprReturn,  // If there was no trap or exception, return
        kExprCatch, js_tag,
          kExprI32Const, 43,
          kExprReturn,
        kExprEnd,
      ])
      .exportFunc();

      builder.addFunction("inner", kSig_i_v)
      .addBody([
        kExprCallFunction, kPromise42Ref,
      ])
      .exportFunc();

    let backChnnl = ()=>instance.exports.inner();
    let instance = builder.instantiate({"": {
        promise42: promise42,
        backChnnl: backChnnl,
        tag: WebAssembly.JSTag,
    }});

    wrapped_export = WebAssembly.promising(instance.exports.main);

    assert_equals(await wrapped_export(), 43);
  },"throw on reentrance");

test(() => {
  let builder = new WasmModuleBuilder();
  import_index = builder.addImport('m', 'import', kSig_v_v);
  builder.addFunction("test", kSig_v_v)
      .addBody([
          kExprCallFunction, import_index,
      ]).exportFunc();
  let js_import = new WebAssembly.Suspending(() => Promise.resolve());
  let instance = builder.instantiate({m: {import: js_import}});
  assert_throws_js(WebAssembly.SuspendError, instance.exports.test);
},"unwrapped export");
