// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --expose-externalize-string
// Flags: --expose-gc --shared-strings

const kChunkSize = 0x40000;
const kResourceOffset = 12;

let shared_buffer = new SharedArrayBuffer(32);
let shared_view = new BigInt64Array(shared_buffer);

function workerCode() {
  const kResourceOffset = 12;
  onmessage = function(msg) {
    let shared_view = new BigInt64Array(msg.data);
    let str = createExternalizableString('B'.repeat(12));
    let str_addr = Sandbox.getAddressOf(str);
    let view = new DataView(new Sandbox.MemoryView(str_addr, 32));

    // Send the string address to main thread.
    Atomics.store(shared_view, 1, BigInt(str_addr));
    Atomics.notify(shared_view, 1, 1);
    Atomics.wait(shared_view, 0, 0n);

    // Externalize string while main thread flips chunk flags at sync point.
    try { externalizeString(str); } catch (e) {}

    // Send the resource handle index to main thread.
    let resource_index = view.getUint32(kResourceOffset, true);
    Atomics.store(shared_view, 1, BigInt(resource_index));
    Atomics.notify(shared_view, 1, 1);

    // Wait until main thread restores chunk flags before running GC.
    Atomics.wait(shared_view, 0, 1n);

    // Free the wrapper object on the worker isolate.
    gc();

    Atomics.store(shared_view, 0, 3n);
    Atomics.notify(shared_view, 0, 1);
    Atomics.wait(shared_view, 0, 3n);
    postMessage('done');
  };
}

let worker = new Worker(workerCode, {type: 'function'});
let main_str = createExternalizableString('C'.repeat(12));
externalizeString(main_str);
let main_str_addr = Sandbox.getAddressOf(main_str);

worker.postMessage(shared_buffer);

// 1. Wait for worker string address.
Atomics.wait(shared_view, 1, 0n);
let str_addr_worker = Number(Atomics.load(shared_view, 1));
Atomics.store(shared_view, 1, 0n);

// 2. Arm synchronization point and signal worker to externalize.
%BlockAt('MakeExternalOneByteString', 10000);
Atomics.store(shared_view, 0, 1n);
Atomics.notify(shared_view, 0, 1);

%WaitUntilBlocked('MakeExternalOneByteString', 10000);

// 3. Flip IN_WRITABLE_SHARED_SPACE flag on worker memory chunk.
let chunk_addr = str_addr_worker - (str_addr_worker % kChunkSize);
let view = new DataView(new Sandbox.MemoryView(chunk_addr, 0x1000));
let flags_low = view.getUint32(0, true);
view.setUint32(0, flags_low | 1, true);

%Resume('MakeExternalOneByteString');

// 4. Wait for worker to finish externalization and get resource index.
Atomics.wait(shared_view, 1, 0n);
let resource_index = Number(Atomics.load(shared_view, 1));

// 5. Restore chunk flags.
view.setUint32(0, flags_low, true);

// 6. Signal worker to run GC.
Atomics.store(shared_view, 0, 2n);
Atomics.notify(shared_view, 0, 1);

// 7. Wait for worker GC to complete.
Atomics.wait(shared_view, 0, 2n);

// 8. Corrupt main string resource handle with the worker resource index.
let main_view = new DataView(new Sandbox.MemoryView(main_str_addr, 32));
main_view.setUint32(kResourceOffset, resource_index, true);

// 9. Access the corrupted string.
try { eval(main_str); } catch (e) {}

Atomics.store(shared_view, 0, 4n);
Atomics.notify(shared_view, 0, 1);
worker.getMessage();
