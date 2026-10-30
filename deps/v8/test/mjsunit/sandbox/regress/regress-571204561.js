// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

function WorkerCode() {
  const kSyncPoint = 'ResolveEvacuationEntryDuringSweeping_HandleRead';

  onmessage = function(e) {
    const {handle_low, high_handle_addrs} = e.data;
    const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

    %BlockAt(kSyncPoint, 10000);
    postMessage('ready');

    %WaitUntilBlocked(kSyncPoint, 10000);

    // Overwrite candidate handle slots with `handle_low` right after the
    // initial read of `*handle_location`.
    for (const addr of high_handle_addrs) {
      memory.setUint32(addr, handle_low, true);
    }

    %Resume(kSyncPoint);
  };
}

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const segmenter = new Intl.Segmenter('en');
const kIcuIteratorOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdOf(segmenter.segment('hello')),
    'icu_iterator_with_text');
const kCppGcWrapperOffset = 4;

function GetHandleAddr(segments_obj) {
  const addr = Sandbox.getAddressOf(segments_obj);
  const managed_addr = memory.getUint32(addr + kIcuIteratorOffset, true) - 1;
  return managed_addr + kCppGcWrapperOffset;
}

// Allocate > 1 MB of table entries and free half to trigger table compaction.
const kNumObjects = 150000;
let hooks = new Array(kNumObjects);
for (let i = 0; i < kNumObjects; i++) {
  hooks[i] = segmenter.segment('hello');
}
gc();

for (let i = 0; i < kNumObjects; i += 2) {
  hooks[i] = null;
}
gc();

const handle_low = memory.getUint32(GetHandleAddr(hooks[1]), true);
const high_handle_addrs = [];
for (let i = 100001; i < kNumObjects; i += 2) {
  high_handle_addrs.push(GetHandleAddr(hooks[i]));
}

const worker = new Worker(WorkerCode, {type: 'function'});
worker.postMessage({handle_low, high_handle_addrs});
assertEquals('ready', worker.getMessage());

// Trigger table compaction and race at `kSyncPoint`.
gc();
worker.terminate();

let evacuated_handle = 0;
for (const addr of high_handle_addrs) {
  const handle = memory.getUint32(addr, true);
  if (handle !== handle_low) {
    evacuated_handle = handle;
    break;
  }
}
assertNotEquals(0, evacuated_handle);

// Drop all references so `evacuated_handle` and its wrapper are collected.
hooks = null;
gc();

const obj_safe = segmenter.segment('hello');
memory.setUint32(GetHandleAddr(obj_safe), evacuated_handle, true);
obj_safe.containing(0);
