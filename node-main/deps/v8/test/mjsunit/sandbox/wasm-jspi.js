// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions for corrupting JSPI objects.

const kHeapObjectTag = 1;
const kJSPromiseType = Sandbox.getInstanceTypeIdFor('JS_PROMISE_TYPE');
const kJSPromiseReactionsOrResultOffset = Sandbox.getFieldOffset(
    kJSPromiseType, 'reactions_or_result');
const kPromiseReaction = Sandbox.getInstanceTypeIdFor('PROMISE_REACTION');
const kPromiseReactionFulfillHandlerOffset = Sandbox.getFieldOffset(
    kPromiseReaction, 'fulfill_handler');
const kJsFunction = Sandbox.getInstanceTypeIdFor('JS_FUNCTION');
const kJSFunctionSharedFunctionInfoOffset = Sandbox.getFieldOffset(
    kJsFunction, 'shared_function_info');
const kSharedFunctionInfo = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO');
const kSharedFunctionInfoFunctionDataOffset = Sandbox.getFieldOffset(
    kSharedFunctionInfo, 'function_data');
const kWasmResumeData = Sandbox.getInstanceTypeIdFor('WASM_RESUME_DATA');
const kWasmResumeDataTrustedSuspenderOffset = Sandbox.getFieldOffset(
    kWasmResumeData, 'trusted_suspender');

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

function get_resume_data(promise) {
  let promise_ptr = getPtr(promise);
  let reaction = getField(promise_ptr, kJSPromiseReactionsOrResultOffset);
  let callback = getField(reaction, kPromiseReactionFulfillHandlerOffset);
  let sfi = getField(callback, kJSFunctionSharedFunctionInfoOffset);
  return getField(sfi, kSharedFunctionInfoFunctionDataOffset);
}

function get_suspender(resume_data) {
  return getField(resume_data, kWasmResumeDataTrustedSuspenderOffset);
}

function set_suspender(resume_data, suspender) {
  setField(resume_data, kWasmResumeDataTrustedSuspenderOffset, suspender);
}
