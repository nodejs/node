// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

// Test for invalid wrappers
test(() => {
  assert_throws_js(TypeError, () => WebAssembly.promising({}),
      "Argument 0 must be a function");
  assert_throws_js(TypeError, () => WebAssembly.promising(() => {}),
      "Argument 0 must be a WebAssembly exported function");
  assert_throws_js(TypeError, () => WebAssembly.Suspending(() => {}),
      "WebAssembly.Suspending must be invoked with 'new'");
  assert_throws_js(TypeError, () => new WebAssembly.Suspending({}),
      "Argument 0 must be a function");

  function asmModule() {
      "use asm";

      function x(v) {
          v = v | 0;
      }
      return x;
  }
  assert_throws_js(TypeError, () => WebAssembly.promising(asmModule()),
      "Argument 0 must be a WebAssembly exported function");
},"Valid use of API");

test(() => {
  let builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true).exportAs('g');
  builder.addFunction("test", kSig_i_v)
      .addBody([
          kExprI32Const, 42,
          kExprGlobalSet, 0,
          kExprI32Const, 0
      ]).exportFunc();
  let instance = builder.instantiate();
  let wrapper = WebAssembly.promising(instance.exports.test);
  wrapper();
  assert_equals(42, instance.exports.g.value);
},"Promising function always entered");

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  let import_index = builder.addImport('m', 'import', kSig_i_v);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprCallFunction, import_index, // suspend
      ]).exportFunc();
  let js_import = () => 42;
  let instance = builder.instantiate({
      m: {
          import: js_import
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  assert_equals(await export_promise, 42);
}, "Always get a Promise");

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  let import_index = builder.addImport('m', 'import', kSig_i_i);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, import_index, // suspend
      ]).exportFunc();
  let js_import = new WebAssembly.Suspending(() => Promise.resolve(42));
  let instance = builder.instantiate({
      m: {
          import: js_import
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  assert_equals(await export_promise, 42);
}, "Suspend once");

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true).exportAs('g');
  let import_index = builder.addImport('m', 'import', kSig_i_v);
  // void test() {
  //   for (i = 0; i < 5; ++i) {
  //     g = g + await import();
  //   }
  // }
  builder.addFunction("test", kSig_v_i)
      .addLocals({
          i32_count: 1
      })
      .addBody([
          kExprI32Const, 5,
          kExprLocalSet, 1,
          kExprLoop, kWasmStmt,
          kExprCallFunction, import_index, // suspend
          kExprGlobalGet, 0,
          kExprI32Add,
          kExprGlobalSet, 0,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
          kExprEnd,
      ]).exportFunc();
  let i = 0;

  function js_import() {
      return Promise.resolve(++i);
  };
  let wasm_js_import = new WebAssembly.Suspending(js_import);
  let instance = builder.instantiate({
      m: {
          import: wasm_js_import
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_equals(instance.exports.g.value, 0);
  assert_true(export_promise instanceof Promise);
  await export_promise;
  assert_equals(instance.exports.g.value, 15);
}, "Suspend/resume in a loop");

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  let import_index = builder.addImport('m', 'import', kSig_i_v);
  builder.addFunction("test", kSig_i_v)
      .addBody([
          kExprCallFunction, import_index, // suspend
      ]).exportFunc();
  let js_import = new WebAssembly.Suspending(() => Promise.resolve(42));
  let instance = builder.instantiate({
      m: {
          import: js_import
      }
  });
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);

  // Also try with a JS function with a mismatching arity.
  js_import = new WebAssembly.Suspending((unused) => Promise.resolve(42));
  instance = builder.instantiate({
      m: {
          import: js_import
      }
  });
  wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);

  // Also try with a proxy.
  js_import = new WebAssembly.Suspending(new Proxy(() => Promise.resolve(42), {}));
  instance = builder.instantiate({
      m: {
          import: js_import
      }
  });
  wrapped_export = WebAssembly.promising(instance.exports.test);
  assert_equals(await wrapped_export(), 42);
},"Suspending with mismatched args and via Proxy");

function recordAbeforeB() {
  let AbeforeB = [];
  let setA = () => {
      AbeforeB.push("A")
  }
  let setB = () => {
      AbeforeB.push("B")
  }
  let isAbeforeB = () =>
      AbeforeB[0] == "A" && AbeforeB[1] == "B";

  let isBbeforeA = () =>
    AbeforeB[0] == "B" && AbeforeB[1] == "A";

  return {
      setA: setA,
      setB: setB,
      isAbeforeB: isAbeforeB,
      isBbeforeA: isBbeforeA,
  }
}

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  let AbeforeB = recordAbeforeB();
  let import42_index = builder.addImport('m', 'import42', kSig_i_i);
  let importSetA_index = builder.addImport('m', 'setA', kSig_v_v);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, import42_index, // suspend?
          kExprCallFunction, importSetA_index
      ]).exportFunc();
  let import42 = new WebAssembly.Suspending(() => Promise.resolve(42));
  let instance = builder.instantiate({
      m: {
          import42: import42,
          setA: AbeforeB.setA
      }
  });

  let wrapped_export = WebAssembly.promising(instance.exports.test);

  let exported_promise = wrapped_export();

  AbeforeB.setB();

  assert_equals(await exported_promise, 42);

  assert_true(AbeforeB.isBbeforeA());
}, "Make sure we actually suspend");

promise_test(async () => {
  let builder = new WasmModuleBuilder();
  let AbeforeB = recordAbeforeB();
  let import42_index = builder.addImport('m', 'import42', kSig_i_i);
  let importSetA_index = builder.addImport('m', 'setA', kSig_v_v);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, import42_index, // suspend?
          kExprCallFunction, importSetA_index
      ]).exportFunc();
  let import42 = new WebAssembly.Suspending(() => 42);
  let instance = builder.instantiate({
      m: {
          import42: import42,
          setA: AbeforeB.setA
      }
  });

  let wrapped_export = WebAssembly.promising(instance.exports.test);

  let exported_promise = wrapped_export();
  AbeforeB.setB();

  assert_equals(await exported_promise, 42);
  assert_true(AbeforeB.isBbeforeA());
}, "Do suspend even if the import's return value is not a Promise by wrapping it with Promise.resolve");

promise_test(async (t) => {
  let tag = new WebAssembly.Tag({
      parameters: ['i32']
  });
  let builder = new WasmModuleBuilder();
  let import_index = builder.addImport('m', 'import', kSig_i_i);
  let tag_index = builder.addImportedTag('m', 'tag', kSig_v_i);
  builder.addFunction("test", kSig_i_i)
      .addBody([
          kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprCallFunction, import_index,
          kExprCatch, tag_index,
          kExprEnd
      ]).exportFunc();

  function js_import(unused) {
      return Promise.reject(new WebAssembly.Exception(tag, [42]));
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
  assert_equals(await export_promise, 42);
}, "Catch rejected promise");

async function TestSandwich(suspend) {
  // Set up a 'sandwich' scenario. The call chain looks like:
  // top (wasm) -> outer (js) -> bottom (wasm) -> inner (js)
  // If 'suspend' is true, the inner JS function returns a Promise, which
  // suspends the bottom wasm function, which returns a Promise, which suspends
  // the top wasm function, which returns a Promise. The inner Promise
  // resolves first, which resumes the bottom continuation. Then the outer
  // promise resolves which resumes the top continuation.
  // If 'suspend' is false, the bottom JS function returns a regular value and
  // no computation is suspended.
  let builder = new WasmModuleBuilder();
  let inner_index = builder.addImport('m', 'inner', kSig_i_i);
  let outer_index = builder.addImport('m', 'outer', kSig_i_i);
  builder.addFunction("top", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, outer_index
      ]).exportFunc();
  builder.addFunction("bottom", kSig_i_i)
      .addBody([
          kExprLocalGet, 0,
          kExprCallFunction, inner_index
      ]).exportFunc();

  let inner = new WebAssembly.Suspending(() => suspend ? Promise.resolve(42) : 43);

  let export_inner;
  let outer = new WebAssembly.Suspending(() => export_inner());

  let instance = builder.instantiate({
      m: {
          inner,
          outer
      }
  });
  export_inner = WebAssembly.promising(instance.exports.bottom);
  let export_top = WebAssembly.promising(instance.exports.top);
  let result = export_top();
  assert_true(result instanceof Promise);
  if (suspend)
      assert_equals(await result, 42);
  else
      assert_equals(await result, 43);
}

promise_test(async () => {
  TestSandwich(true);
}, "Test sandwich with suspension");

promise_test(async () => {
  TestSandwich(false);
}, "Test sandwich with no suspension");

test(() => {
  // Check that a promising function with no return is allowed.
  let builder = new WasmModuleBuilder();
  builder.addFunction("export", kSig_v_v).addBody([]).exportFunc();
  let instance = builder.instantiate();
  let export_wrapper = WebAssembly.promising(instance.exports.export);
  assert_true(export_wrapper instanceof Function);
},"Promising with no return");

promise_test(async () => {
  let builder1 = new WasmModuleBuilder();
  let import_index = builder1.addImport('m', 'import', kSig_i_v);
  builder1.addFunction("f", kSig_i_v)
      .addBody([
          kExprCallFunction, import_index, // suspend
          kExprI32Const, 1,
          kExprI32Add,
      ]).exportFunc();
  let js_import = new WebAssembly.Suspending(() => Promise.resolve(1));
  let instance1 = builder1.instantiate({
      m: {
          import: js_import
      }
  });
  let builder2 = new WasmModuleBuilder();
  import_index = builder2.addImport('m', 'import', kSig_i_v);
  builder2.addFunction("main", kSig_i_v)
      .addBody([
          kExprCallFunction, import_index,
          kExprI32Const, 1,
          kExprI32Add,
      ]).exportFunc();
  let instance2 = builder2.instantiate({
      m: {
          import: instance1.exports.f
      }
  });
  let wrapped_export = WebAssembly.promising(instance2.exports.main);
  assert_equals(await wrapped_export(), 3);
},"Suspend two modules");