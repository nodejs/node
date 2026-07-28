// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/jspi/testharness-additions.js

promise_test(t => {
  let tag = new WebAssembly.Tag({
      parameters: []
  });
  let builder = new WasmModuleBuilder();
  import_index = builder.addImport('m', 'import', kSig_i_i);
  tag_index = builder.addImportedTag('m', 'tag', kSig_v_v);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, import_index,
          kExprThrow, tag_index
      ]).exportFunc();

  function js_import() {
      return Promise.resolve();
  };
  let wasm_js_import = new WebAssembly.Suspending(js_import);

  let instance = builder.instantiate({
      m: {
          import: wasm_js_import,
          tag: tag
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  return promise_rejects_jspi(t, new WebAssembly.Exception(tag, []), export_promise);
}, "Throw after the first suspension");

// Throw an exception before suspending. The export wrapper should return a
// promise rejected with the exception.
promise_test(async (t) => {
  let tag = new WebAssembly.Tag({
      parameters: []
  });
  let builder = new WasmModuleBuilder();
  tag_index = builder.addImportedTag('m', 'tag', kSig_v_v);
  builder.addFunction("test", kSig_i_v)
      .addBody([
          kExprThrow, tag_index
      ]).exportFunc();

  let instance = builder.instantiate({
      m: {
          tag: tag
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();

  promise_rejects_jspi(t, new WebAssembly.Exception(tag, []), export_promise);
}, "Throw before suspending");

// Throw an exception after the first resume event, which propagates to the
// promise wrapper.
promise_test(async (t) => {
  let tag = new WebAssembly.Tag({
      parameters: []
  });
  let builder = new WasmModuleBuilder();
  import_index = builder.addImport('m', 'import', kSig_i_v);
  tag_index = builder.addImportedTag('m', 'tag', kSig_v_v);
  builder.addFunction("test", kSig_i_v)
      .addBody([
          kExprCallFunction, import_index,
          kExprThrow, tag_index
      ]).exportFunc();

  function js_import() {
      return Promise.resolve(42);
  };
  let wasm_js_import = new WebAssembly.Suspending(js_import);

  let instance = builder.instantiate({
      m: {
          import: wasm_js_import,
          tag: tag
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();

  promise_rejects_jspi(t, new WebAssembly.Exception(tag, []), export_promise);
}, "Throw and propagate via Promise");

promise_test(async (t) => {
  let builder = new WasmModuleBuilder();
  builder.addFunction("test", kSig_i_v)
      .addBody([
          kExprCallFunction, 0
      ]).exportFunc();
  let instance = builder.instantiate();
  let wrapper = WebAssembly.promising(instance.exports.test);

  // Stack overflow has undefined behavior -- check if an exception was thrown.
  let thrown = false;
  try {
    await wrapper();
  } catch (e) {
    thrown = true;
  }
  assert_true(thrown);
}, "Stack overflow");

promise_test(async (t) => {
  // The call stack of this test looks like:
  // export1 -> import1 -> export2 -> import2
  // Where export1 is "promising" and import2 is "suspending". Returning a
  // promise from import2 should trap because of the JS import in the middle.
  let builder = new WasmModuleBuilder();
  let import1_index = builder.addImport("m", "import1", kSig_i_v);
  let import2_index = builder.addImport("m", "import2", kSig_i_v);
  builder.addFunction("export1", kSig_i_v)
      .addBody([
          // export1 -> import1 (unwrapped)
          kExprCallFunction, import1_index,
      ]).exportFunc();
  builder.addFunction("export2", kSig_i_v)
      .addBody([
          // export2 -> import2 (suspending)
          kExprCallFunction, import2_index,
      ]).exportFunc();
  let instance;

  function import1() {
      // import1 -> export2 (unwrapped)
      instance.exports.export2();
  }

  function import2() {
      return Promise.resolve(0);
  }
  import2 = new WebAssembly.Suspending(import2);
  instance = builder.instantiate({
      'm': {
          'import1': import1,
          'import2': import2
      }
  });
  // export1 (promising)
  let wrapper = WebAssembly.promising(instance.exports.export1);
  promise_rejects_js(t, WebAssembly.SuspendError, wrapper(),
      "trying to suspend JS frames");
}, "Try to suspend JS");