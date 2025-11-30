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
let jspi_suspending_index = builder.addImport("m", "jspi_suspending", sig_v_v);
builder.addExport("jspi_suspending", jspi_suspending_index);
let call_next_from_js_index =
    builder.addImport("m", "call_next_from_js", kSig_v_v);
builder.addExport("call_next_from_js", call_next_from_js_index);
let get_next_sig = builder.addType(makeSig([], [wasmRefNullType(sig_v_v)]));
let get_next_index = builder.addImport("m", "get_next", get_next_sig);
let tag0_index = builder.addTag(kSig_v_v);
let tag1_index = builder.addTag(kSig_v_v);
// A list of wasm functions where each function is expected to pop and call
// the next one in the list, to easily create specific call stacks.
let call_stack = builder.addGlobal(kWasmAnyRef, true).exportAs("call_stack");
let rejecting_promise_index = builder.addImport(
    "m", "rejecting_promise", sig_v_v);
builder.addExport("rejecting_promise", rejecting_promise_index);
let g_cont = builder.addGlobal(wasmRefNullType(cont_index), true, false);
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
    .addBody([kExprThrow, tag0_index]).exportFunc();
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
let nop_index = builder.addFunction("nop", kSig_v_v).addBody([]).exportFunc().index;
builder.addFunction("resume_next_handle_tag0", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprCallFunction, get_next_index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, tag0_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprDrop,
    ]).exportFunc();
builder.addFunction("resume_next_handle_tag1", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprCallFunction, get_next_index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, tag1_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprDrop,
        kExprUnreachable,
    ]).exportFunc();
builder.addFunction("resume_next_twice", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprCallFunction, get_next_index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, tag0_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprResume, cont_index, 0,
    ]).exportFunc();
builder.addFunction("resume_next_with_handler_and_catch_all", kSig_i_v)
    .addBody([
        kExprBlock, kWasmVoid,
          kExprTryTable, kWasmVoid, 1,
          kCatchAllNoRef, 0,
            kExprBlock, kWasmRef, cont_index,
              kExprCallFunction, get_next_index,
              kExprContNew, cont_index,
              kExprResume, cont_index, 1, kOnSuspend, tag0_index, 0,
              kExprI32Const, 10,
              kExprReturn,
            kExprEnd,
            kExprDrop,
            kExprI32Const, 11,
            kExprReturn,
          kExprEnd,
          kExprUnreachable,
        kExprEnd,
        kExprI32Const, 12,
        kExprReturn,
    ]).exportFunc();
builder.addFunction("throw_exn", kSig_v_v)
    .addBody([
        kExprThrow, tag0_index
    ]).exportFunc();
let suspend_tag0 = builder.addFunction("suspend_tag0", kSig_v_v)
    .addBody([
        kExprSuspend, tag0_index
    ]).exportFunc();
builder.addFunction("suspend_tag1", kSig_v_v)
    .addBody([
        kExprSuspend, tag1_index
    ]).exportFunc();
builder.addFunction("resume_bad_cont_1", kSig_v_v)
    .addBody([
          kExprRefFunc, nop_index,
          kExprContNew, cont_index,
          kExprGlobalSet, g_cont.index,
          kExprGlobalGet, g_cont.index,
          kExprResume, cont_index, 0,
          // Reusing cont after return.
          kExprGlobalGet, g_cont.index,
          kExprResume, cont_index, 0
    ]).exportFunc();
builder.addFunction("resume_bad_cont_2", kSig_v_v)
    .addBody([
          kExprBlock, kWasmRef, cont_index,
            kExprRefFunc, suspend_tag0.index,
            kExprContNew, cont_index,
            kExprGlobalSet, g_cont.index,
            kExprGlobalGet, g_cont.index,
            kExprResume, cont_index, 1, kOnSuspend, tag0_index, 0,
            kExprReturn,
          kExprEnd,
          kExprDrop,
          // Reusing cont after suspend.
          kExprGlobalGet, g_cont.index,
          kExprResume, cont_index, 0
    ]).exportFunc();
let helper = builder.addFunction("resume_bad_cont_3_helper", kSig_v_v)
    .addBody([
        kExprGlobalGet, g_cont.index,
        // Reusing cont before return/suspend.
        kExprResume, cont_index, 0,
    ]).exportFunc();
builder.addFunction("resume_bad_cont_3", kSig_v_v)
    .addLocals(wasmRefNullType(cont_index), 1)
    .addBody([
          kExprRefFunc, helper.index,
          kExprContNew, cont_index,
          kExprGlobalSet, g_cont.index,
          kExprGlobalGet, g_cont.index,
          kExprResume, cont_index, 0
    ]).exportFunc();
builder.addFunction("resume_next_with_two_handlers", kSig_i_v)
    .addBody([
        kExprBlock, kWasmVoid,
          kExprBlock, kWasmRef, cont_index,
            kExprBlock, kWasmRef, cont_index,
              kExprCallFunction, get_next_index,
              kExprContNew, cont_index,
              kExprResume, cont_index, 2,
                kOnSuspend, tag0_index, 0,
                kOnSuspend, tag1_index, 1,
              kExprUnreachable,
            kExprEnd,
            kExprDrop,
            kExprI32Const, 0,
            kExprReturn,
          kExprEnd,
          kExprDrop,
          kExprI32Const, 1,
          kExprReturn,
        kExprEnd,
        kExprUnreachable,
    ]).exportFunc();
builder.addFunction("resume_next_with_two_handlers_same_tag", kSig_i_v)
    .addBody([
        kExprBlock, kWasmVoid,
          kExprBlock, kWasmRef, cont_index,
            kExprBlock, kWasmRef, cont_index,
              kExprCallFunction, get_next_index,
              kExprContNew, cont_index,
              kExprResume, cont_index, 2,
                kOnSuspend, tag0_index, 0,
                kOnSuspend, tag0_index, 1,
              kExprUnreachable,
            kExprEnd,
            kExprDrop,
            kExprI32Const, 0,
            kExprReturn,
          kExprEnd,
          kExprDrop,
          kExprI32Const, 1,
          kExprReturn,
        kExprEnd,
        kExprUnreachable,
    ]).exportFunc();
let instance;
instance = builder.instantiate( {m: {
  gc,
  jspi_suspending: new WebAssembly.Suspending(() => {}),
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
      instance.exports.jspi_suspending
  ];
  WebAssembly.promising(instance.exports.call_next)();

  // Throw if the top WasmFX stack contains JS frames:
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.jspi_suspending
  ];
  assertThrowsAsync(
      WebAssembly.promising(instance.exports.call_next_as_cont)(),
      WebAssembly.SuspendError);

  // Throw if an intermediate stack contains JS frames.
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.call_next_as_cont,
      instance.exports.jspi_suspending
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

(function TestSuspend() {
  print(arguments.callee.name);

  instance.exports.call_stack.value = [
      instance.exports.suspend_tag0
  ];
  instance.exports.resume_next_twice();

  // A resume instruction within a try scope triggers interesting code paths:
  // the resume builtin call must be able to handle either exceptions or
  // effects.
  // Test the three possible successors of the resume instruction: normal
  // return, exception handler and effect handler.
  instance.exports.call_stack.value = [
      instance.exports.nop
  ];
  assertEquals(10, instance.exports.resume_next_with_handler_and_catch_all());

  instance.exports.call_stack.value = [
      instance.exports.suspend_tag0
  ];
  assertEquals(11, instance.exports.resume_next_with_handler_and_catch_all());

  instance.exports.call_stack.value = [
      instance.exports.throw_exn
  ];
  assertEquals(12, instance.exports.resume_next_with_handler_and_catch_all());

  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_as_cont,
      instance.exports.suspend_tag0,
  ];
  instance.exports.resume_next_twice();

  instance.exports.call_stack.value = [
      instance.exports.resume_next_handle_tag1,
      instance.exports.suspend_tag0
  ];
  instance.exports.resume_next_handle_tag0();
})();

(function TestSuspendError() {
  print(arguments.callee.name);
  // Throw if the top WasmFX stack contains JS frames:
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.suspend_tag0,
  ];
  assertThrows(instance.exports.resume_next_handle_tag0,
      WebAssembly.SuspendError);

  // Throw if an intermediate stack contains JS frames.
  instance.exports.call_stack.value = [
      instance.exports.call_next_as_cont,
      instance.exports.call_next_from_js,
      instance.exports.call_next_as_cont,
      instance.exports.suspend_tag0,
  ];
  assertThrows(instance.exports.resume_next_handle_tag0,
      WebAssembly.SuspendError);

  instance.exports.call_stack.value = [
      instance.exports.suspend_tag1];
  assertThrows(instance.exports.resume_next_handle_tag0,
      WebAssembly.SuspendError);
})();

(function TestInvalidContinuation() {
  print(arguments.callee.name);
  assertThrows(instance.exports.resume_bad_cont_1, WebAssembly.RuntimeError);
  assertThrows(instance.exports.resume_bad_cont_2, WebAssembly.RuntimeError);
  assertThrows(instance.exports.resume_bad_cont_3, WebAssembly.RuntimeError);
})();

(function TestMultipleHandlers() {
  print(arguments.callee.name);
  instance.exports.call_stack.value = [instance.exports.suspend_tag0];
  assertEquals(0, instance.exports.resume_next_with_two_handlers());
  instance.exports.call_stack.value = [instance.exports.suspend_tag1];
  assertEquals(1, instance.exports.resume_next_with_two_handlers());

  // If the same tag is used multiple times, use the first occurrence.
  instance.exports.call_stack.value = [instance.exports.suspend_tag0];
  assertEquals(0, instance.exports.resume_next_with_two_handlers_same_tag());
})();

(function TestResumeSuspendReturn() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let cont_index = builder.addCont(kSig_v_i);
  let tag_index = builder.addTag(kSig_i_v);
  let suspend_if = builder.addFunction('suspend_if', kSig_v_i)
      .addBody([
          kExprLocalGet, 0,
          kExprIf, kWasmVoid,
            kExprSuspend, tag_index,
            kExprDrop,
          kExprEnd,
      ]).exportFunc();
  const kSuspended = 0;
  const kReturned = 1;
  builder.addFunction("main", kSig_i_i)
      .addBody([
          kExprBlock, kWasmRef, cont_index,
            kExprLocalGet, 0,
            kExprRefFunc, suspend_if.index,
            kExprContNew, cont_index,
            kExprResume, cont_index, 1, kOnSuspend, tag_index, 0,
            kExprI32Const, kReturned,
            kExprReturn,
          kExprEnd,
          kExprDrop,
          kExprI32Const, kSuspended,
      ]).exportFunc();
  assertTrue(WebAssembly.validate(builder.toBuffer()));
  // let instance = builder.instantiate();
  // assertEquals(kReturned, instance.exports.main(0));
  // assertEquals(kSuspended, instance.exports.main(1));
})();
