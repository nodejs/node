// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

// Regression test for an out-of-bounds tagged write in the DrumBrake
// interpreter after a failed same-runtime reentry. When a reentry fails in
// WasmInterpreterRuntime::BeginExecution because the interpreter stack cannot
// be grown, the ExpandStack failure path removed the just-started activation
// via FinishActivation() but did not restore current_frame_ (unlike the normal
// teardown in ContinueExecution). The suspended outer frame then resumed with
// the failed inner activation's stale ref_array_current_sp_, so StoreWasmRef
// added a stale base to the bytecode-provided reference index and wrote out of
// bounds of the reference-stack FixedArray.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Each `ref.null extern; drop` gets a distinct eight-byte interpreter slot.
// The interpreter frame must exceed 16 MiB so that a same-runtime reentry
// begins beyond the 32 MiB reservation and fails. 2*2^20 + 1 slots gives a
// frame just past 16 MiB.
//
// The 16-vs-32 MiB factor of two comes from how the reentry's frame base is
// computed. ExecuteImportedFunction calls SetCurrentActivationFrame with
// `current_sp_ + slot_offset` as the frame pointer and `slot_offset` as the
// frame size, so WasmInterpreterThread::NextFrameAddress(), which returns
// `current_fp_ + current_frame_size_`, hands the reentry a base of
// `current_sp_ + 2 * slot_offset`. An outer frame just over 16 MiB therefore
// starts the nested activation past the 32 MiB kMaxStackSize reservation, and
// ExpandStack fails.
const kFillerRefCount = 2 * 1024 * 1024 + 1;

const builder = new WasmModuleBuilder();
const sig_v_v = builder.addType(kSig_v_v);

// Imported function that reenters the interpreter.
const kReenterImport = builder.addImport('m', 'reenter', sig_v_v);

// An empty export used as the same-instance reentry target.
builder.addFunction('inner', sig_v_v).addBody([]).exportFunc();

// The outer function whose suspended frame resumes after the failed reentry.

// An outer () -> () function performs enough `ref.null extern; drop` sequences
// that the interpreter frame is larger than half of the interpreter's 32 MiB
// stack reservation. It then makes an imported call. JavaScript reenters
// another export of the same instance; that nested entry overflows the
// reservation and fails, and JavaScript catches the resulting RangeError. When
// the imported call returns, the outer function resumes and a trailing
// reference-producing instruction performs the tagged store.
const runBody = new Array(kFillerRefCount * 3 + 6);
let p = 0;
for (let i = 0; i < kFillerRefCount; ++i) {
  runBody[p++] = kExprRefNull;
  runBody[p++] = kExternRefCode;
  runBody[p++] = kExprDrop;
}
runBody[p++] = kExprCallFunction;
runBody[p++] = kReenterImport;
runBody[p++] = kExprRefNull;
runBody[p++] = kExternRefCode;
runBody[p++] = kExprDrop;
runBody[p++] = kExprEnd;
builder.addFunction('run', sig_v_v).addBodyWithEnd(runBody).exportFunc();

let instance;
let caught = false;
instance = builder.instantiate({
  m: {
    reenter() {
      try {
        // Same-instance reentry. This nested entry overflows DrumBrake's stack
        // reservation and must fail with a RangeError, which we catch.
        instance.exports.inner();
      } catch (e) {
        caught = true;
      }
    },
  },
});

// Without the fix the resumed outer frame performs an out-of-bounds tagged
// write here and crashes. With the fix it completes normally.
instance.exports.run();

assertTrue(caught, 'nested same-runtime reentry should have overflowed');
