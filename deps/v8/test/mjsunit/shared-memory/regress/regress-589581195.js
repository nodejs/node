// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --shared-heap --expose-externalize-string --stress-compaction --expose-gc

// Populate >1 segment (8192 entries per 64KB segment) of
// shared_external_pointer_space() so the second segment becomes an evacuation
// candidate during shared EPT compaction.
const keep = Array.from({length: 8200}, () => {
  const s = createExternalizableString('a'.repeat(32));
  externalizeString(s);
  return s;
});

// Create a Worker with an ExternalString in its local OLD_SPACE whose handle
// lands in segment 2 of shared_external_pointer_space().
const worker = new Worker(function() {
  let s = createExternalizableString('a'.repeat(32));
  externalizeString(s);
  postMessage('ready');
  onmessage = function() {
    // 1. Local major GC while shared_external_pointer_space() is compacting
    //    (must not record `s`'s Worker-local slot in the shared EPT).
    gc();
    // 2. Drop `s` so the `gc()` below frees it and sweeps its OLD_SPACE slot
    //    onto the freelist.
    s = null;
    gc();
    // 3. Promote non-handle data into the freed OLD_SPACE slot for Release
    //    builds that do not zap swept memory.
    const reclaim = Array.from({length: 100}, () => [1.1, 2.2]);
    gc();
    postMessage('done');
  };
}, {type: 'function'});
assertEquals('ready', worker.getMessage());

// Sweep 100 segment-1 entries onto the freelist so compaction has room below
// the evacuation frontier, then start incremental marking on the shared-space
// isolate (enabling shared EPT compaction without holding a global safepoint).
keep.fill(null, 0, 100);
gc();
const trigger = new ArrayBuffer(16 * 1024 * 1024);

worker.postMessage('run');
assertEquals('done', worker.getMessage());

// Finalize the shared GC and sweep shared_external_pointer_table().
gc();
worker.terminate();
