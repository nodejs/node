// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_index = builder.addCont(sig_v_v);
let gc_index = builder.addImport("m", "gc", kSig_v_v);
builder.addExport("gc", gc_index);
let suspending_index = builder.addImport("m", "suspending", sig_v_v);
builder.addExport("suspending", suspending_index);
let call_next_from_js_index =
    builder.addImport("m", "call_next_from_js", kSig_v_v);
builder.addExport("call_next_from_js", call_next_from_js_index);
let get_next_sig = builder.addType(makeSig([], [wasmRefNullType(sig_v_v)]));
let get_next_index = builder.addImport("m", "get_next", get_next_sig);
let tag_index = builder.addTag(kSig_v_v);
// A list of wasm functions where each function is expected to pop and call
// the next one in the list, to easily create specific call stacks.
let call_stack = builder.addGlobal(kWasmAnyRef, true).exportAs("call_stack");
let rejecting_promise_index = builder.addImport(
    "m", "rejecting_promise", sig_v_v);
builder.addExport("rejecting_promise", rejecting_promise_index);
builder.addFunction("cont_new_null", kSig_v_v)
    .addBody([
        kExprRefNull, kNullFuncRefCode,
        kExprContNew, cont_index,
        kExprUnreachable,
    ]).exportFunc();
builder.addFunction("resume_null", kSig_v_v)
    .addBody([
        kExprRefNull, kNullContRefCode,
        kExprResume, cont_index,
        kExprUnreachable,
    ]).exportFunc();
builder.addFunction("throw_error", kSig_v_v)
    .addBody([kExprThrow, tag_index]).exportFunc();
builder.addFunction("call_next_as_cont", kSig_v_v)
    .addBody([
        kExprCallFunction, get_next_index,
        kExprContNew, cont_index,
        kExprResume, cont_index, 0,
    ]).exportFunc();
builder.addFunction("call_next", kSig_v_v)
    .addBody([
        kExprCallFunction, get_next_index,
        kExprCallRef, sig_v_v,
    ]).exportFunc();
builder.addFunction("call_next_in_catch_all", kSig_v_v)
    .addBody([
      kExprTryTable, kWasmVoid, 1,
      kCatchAllNoRef, 0,
        kExprCallFunction, get_next_index,
        kExprCallRef, sig_v_v,
      kExprEnd,
    ]).exportFunc();
let instance;
instance = builder.instantiate( {m: {
  gc,
  suspending: new WebAssembly.Suspending(() => {}),
  call_next_from_js: () => instance.exports.call_stack.value.shift()(),
  get_next: () => instance.exports.call_stack.value.shift(),
  rejecting_promise: new WebAssembly.Suspending(() => Promise.reject())
}});

(function TestNullRefs() {
  print(arguments.callee.name);
  assertThrows(instance.exports.cont_new_null, WebAssembly.RuntimeError);
  assertThrows(instance.exports.resume_null, WebAssembly.RuntimeError);
})();

(function TestInitialResumeAndReturn() {
  print(arguments.callee.name);
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.gc
  ];
  instance.exports.call_next();

  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_as_cont,
      instance.exports.call_next_as_cont,
      instance.exports.gc,
  ];
  instance.exports.call_next();

  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_as_cont,
      instance.exports.call_next_as_cont,
      instance.exports.throw_error,
  ];
  assertThrows(instance.exports.call_next, WebAssembly.Exception);
})();

(function TestWasmFXResumeAndJSPISuspend() {
  print(arguments.callee.name);
  // Suspend multiple WasmFX stacks with JSPI.
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.suspending
  ];
  WebAssembly.promising(instance.exports.call_next)();

  // Throw if the top WasmFX stack contains JS frames:
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.suspending
  ];
  assertThrowsAsync(
      WebAssembly.promising(instance.exports.call_next_as_cont)(),
      WebAssembly.SuspendError);

  // Throw if an intermediate stack contains JS frames.
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.call_next_as_cont,
      instance.exports.suspending
  ];
  assertThrowsAsync(
      WebAssembly.promising(instance.exports.call_next_as_cont)(),
      WebAssembly.SuspendError);

  // Catch error in an intermediate stack.
  instance.exports.call_stack.value = [
      instance.exports.call_next_in_catch_all,
      instance.exports.call_next_as_cont,
      instance.exports.rejecting_promise,
  ];
  WebAssembly.promising(instance.exports.call_next_as_cont)()
})();

(function TestResumeSuspendReturn() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let cont_index = builder.addCont(kSig_v_i);
  let tag_index = builder.addTag(kSig_v_v);
  let suspend_if = builder.addFunction('suspend_if', kSig_v_i)
      .addBody([
          kExprLocalGet, 0,
          kExprIf, kWasmVoid,
            kExprSuspend, tag_index,
          kExprEnd,
      ]).exportFunc();
  const kSuspended = 0;
  const kReturned = 1;
  builder.addFunction("main", kSig_i_i)
      .addBody([
          kExprBlock, kWasmVoid,
            kExprLocalGet, 0,
            kExprRefFunc, suspend_if.index,
            kExprContNew, cont_index,
            kExprResume, cont_index, 1, kOnSuspend, tag_index, 0,
            kExprI32Const, kReturned,
            kExprReturn,
          kExprEnd,
          kExprI32Const, kSuspended,
      ]).exportFunc();
  assertTrue(WebAssembly.validate(builder.toBuffer()));
  // let instance = builder.instantiate();
  // assertEquals(kReturned, instance.exports.main(0));
  // assertEquals(kSuspended, instance.exports.main(1));
})();
