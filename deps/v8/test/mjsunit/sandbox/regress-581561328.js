// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax
// Flags: --turbofan --no-turbolev --no-optimize-on-next-call-optimizes-to-maglev

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const field = (type, name) =>
    Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor(type), name);
const dispatchOffset = field('JS_FUNCTION_TYPE', 'dispatch_handle');
const sharedOffset = field('JS_FUNCTION_TYPE', 'shared_function_info');
const formalCountOffset =
    field('SHARED_FUNCTION_INFO_TYPE', 'formal_parameter_count');
const cellOffset = field('JS_FUNCTION_TYPE', 'feedback_cell');
const valueOffset = field('FEEDBACK_CELL_TYPE', 'value');

function address(object) {
  return Sandbox.getAddressOf(object) >>> 0;
}
function read32(location) {
  return memory.getUint32(location, true);
}
function write32(location, value) {
  memory.setUint32(location, value >>> 0, true);
}
function dispatch(fn) {
  return read32(address(fn) + dispatchOffset);
}
function shared(fn) {
  return (read32(address(fn) + sharedOffset) - 1) >>> 0;
}
function cell(fn) {
  return (read32(address(fn) + cellOffset) - 1) >>> 0;
}
function setDispatch(fn, handle) {
  write32(address(fn) + dispatchOffset, handle);
}
function setFormalCount(fn, value) {
  memory.setUint16(shared(fn) + formalCountOffset, value, true);
}

const undefinedTagged = (address(undefined) + 1) >>> 0;
const retained = [];

function raceSourceTarget() {
  return 7;
}
function reader() {
  return raceSourceTarget();
}
function makeTarget(q0, q1, q2, q3, q4, q5, q6, q7) {
  let target;
  for (let i = 0; i < 2; ++i) {
    target = function targetChild(q0, q1, q2, q3, q4, q5, q6, q7) {
      q5 = q4;
      return 0x4242;
    };
  }
  const spare = function(q0, q1, q2, q3, q4, q5, q6, q7) {};
  return [spare, target];
}

%NeverOptimizeFunction(raceSourceTarget);
%EnsureFeedbackVectorForFunction(raceSourceTarget);
%NeverOptimizeFunction(makeTarget);
%PrepareFunctionForOptimization(reader);
reader();
reader();

const initialPair = makeTarget();
let donor = initialPair[0];
const initialTarget = initialPair[1];
%NeverOptimizeFunction(initialTarget);
%CompileBaseline(initialTarget);
initialTarget();
retained.push(donor, initialTarget);

function newTarget() {
  write32(cell(makeTarget) + valueOffset, undefinedTagged);
  setDispatch(makeTarget, dispatch(donor));
  const pair = makeTarget();
  donor = pair[0];
  retained.push(pair[0], pair[1]);
  return pair[1];
}
%NeverOptimizeFunction(newTarget);

const sab = new SharedArrayBuffer(4);
const sync = new Int32Array(sab);
const worker = new Worker(function() {
  onmessage = function(msg) {
    const sync = new Int32Array(msg.data ?? msg);
    Atomics.store(sync, 0, 1);
    Atomics.notify(sync, 0);
    Atomics.wait(sync, 0, 1);

    %BlockAt("JSDispatchEntryMake", 10000);
    %BlockAt("TurboshaftAfterAssembleCode", 10000);
    Atomics.store(sync, 0, 3);
    Atomics.notify(sync, 0);

    %WaitUntilBlocked("JSDispatchEntryMake", 10000);
    %Resume("TurboshaftAssembleCode");
    %WaitUntilBlocked("TurboshaftAfterAssembleCode", 10000);
    %Resume("TurboshaftAfterAssembleCode");
    %Resume("JSDispatchEntryMake");
  };
}, {type: 'function'});

worker.postMessage(sab);
Atomics.wait(sync, 0, 0);

newTarget();
while (((dispatch(donor) >>> 8) & 2047) >= 2045) {
  newTarget();
}
const wantedHandle = ((dispatch(donor) >>> 8) + 1) << 8;

setFormalCount(raceSourceTarget, 0);
setDispatch(raceSourceTarget, dispatch(Number));

%BlockAt("TurboshaftAssembleCode", 10000);
%OptimizeFunctionOnNextCall(reader, "concurrent");
reader();
%WaitUntilBlocked("TurboshaftAssembleCode", 10000);

setDispatch(raceSourceTarget, wantedHandle);

Atomics.store(sync, 0, 2);
Atomics.notify(sync, 0);
Atomics.wait(sync, 0, 2);

newTarget();

%WaitForBackgroundOptimization();
memory.setInt32(cell(raceSourceTarget) + valueOffset + 8, 0x10000000, true);
reader();
