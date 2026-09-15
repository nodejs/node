// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --lazy

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

// A suspended JSPI computation supplies genuine WasmResumeData.
const pendingPromises = [];
function suspend() {
  const promise = new Promise(() => {});
  pendingPromises.push(promise);
  return promise;
}

const builder = new WasmModuleBuilder();
const suspendImport = builder.addImport('m', 'suspend', kSig_v_v);
builder.addFunction('run', kSig_v_v)
    .addBody([
      kExprCallFunction,
      suspendImport,
      kExprCallFunction,
      suspendImport,
    ])
    .exportFunc();
const instance =
    builder.instantiate({m: {suspend: new WebAssembly.Suspending(suspend)}});
const outerPromise = WebAssembly.promising(instance.exports.run)();
const resumeData = get_resume_data(pendingPromises[0]);

// Prewarm the carrier so CompileLazy can install code without compiling its
// source. The zero-arity donor remains lazy and supplies a genuine JDT entry
// whose trusted parameter count is one (the receiver).
function carrier() {
  return 0x43;
}
function zeroArityLazyDonor() {
  return 0x44;
}
carrier();

const jsFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION');
const sharedFunctionInfoType =
    Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO');
const dispatchHandleOffset =
    Sandbox.getFieldOffset(jsFunctionType, 'dispatch_handle');
const sharedFunctionInfoOffset =
    Sandbox.getFieldOffset(jsFunctionType, 'shared_function_info');
const trustedFunctionDataOffset =
    Sandbox.getFieldOffset(sharedFunctionInfoType, 'trusted_function_data');
const functionDataOffset =
    Sandbox.getFieldOffset(sharedFunctionInfoType, 'function_data');

const carrierPtr = getPtr(carrier);
const donorPtr = getPtr(zeroArityLazyDonor);
const carrierSfi = getField(carrierPtr, sharedFunctionInfoOffset);
const donorDispatchHandle = getField(donorPtr, dispatchHandleOffset);

// Keep the destination JDT count from the lazy donor, but select WasmResume
// from the carrier SFI's genuine resume data.
setField(carrierPtr, dispatchHandleOffset, donorDispatchHandle);
setField(carrierSfi, trustedFunctionDataOffset, 0);
setField(carrierSfi, functionDataOffset, resumeData);

globalThis.keepAlive =
    [pendingPromises, outerPromise, instance, carrier, resumeData];

// The mismatched JDT update must fail compatibility verification. On a
// vulnerable build, CompileLazy installs WasmResume into the count-1 entry.
// One explicit argument balances WasmResume's fixed cleanup, so execution
// returns here and makes the regression deterministic.
carrier(undefined);

assertUnreachable('A mismatched JDT entry accepted WasmResume');
