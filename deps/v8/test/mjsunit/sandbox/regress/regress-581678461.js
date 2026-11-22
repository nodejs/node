// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax
// Flags: --no-memory-pool --no-parallel-compaction --max-old-space-size=2054

const kChunkMask = ~0x3ffff;
const kNotNormalOldPageMask = 0x98;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const pageOf = obj => Sandbox.getAddressOf(obj) & kChunkMask;
const flagsOf = page => mem.getUint32(page, true);
const elementsOf = obj => Sandbox.readObjectField(obj, 'elements') & ~1;
const kDoubleLengthOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('FIXED_DOUBLE_ARRAY_TYPE'), 'length');

function workerCode() {
  const kSyncPoint = 'ConcurrentMarkerMajorBgThread';
  const kEvacuationCandidateFlag = 0x100;

  onmessage = function({data: {pages, capacityField}}) {
    const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

    %BlockAt(kSyncPoint, 10000);
    postMessage('ready');

    %WaitUntilBlocked(kSyncPoint, 10000);

    for (const page of pages) {
      const flags = mem.getUint32(page, true);
      if (flags & kEvacuationCandidateFlag) {
        mem.setUint32(page, flags & ~kEvacuationCandidateFlag, true);
      }
    }
    mem.setUint32(capacityField, 0x0ffffff0, true);

    %Resume(kSyncPoint);
    postMessage('done');
  };
}

const large = new Array(20000).fill(1.1);
const youngHolder = [null];
let population = [];
for (let i = 0; i < 300000; i++) {
  population.push({a: i, b: i + 1, c: i + 2, d: i + 3});
}
gc();
gc();

const pageObjs = new Map();
for (const obj of population) {
  const page = pageOf(obj);
  if (flagsOf(page) & kNotNormalOldPageMask) continue;
  let list = pageObjs.get(page);
  if (!list) pageObjs.set(page, list = []);
  list.push(obj);
}
population = null;

const fullPages = Array.from(pageObjs.entries())
                      .filter(([, list]) => list.length >= 7000)
                      .map(([page]) => page)
                      .slice(1, -1);
const nonCandPages = new Set(fullPages.slice(0, 5));
const pages = fullPages.slice(5);

let anchors = [];
let lateObjs = [];
for (const [page, list] of pageObjs.entries()) {
  if (nonCandPages.has(page)) {
    lateObjs.push(list);
  } else {
    for (let i = 0; i < list.length; i += 64) anchors.push(list[i]);
  }
}
pageObjs.clear();
gc();

for (let idx = 0; idx < lateObjs.length; idx++) {
  const list = lateObjs[idx];
  const step = idx === 0 ? 128 : 16;
  for (let i = 0; i < list.length; i += step) anchors.push(list[i]);
}
lateObjs = null;

const capacityField = elementsOf(large) + kDoubleLengthOffset;
const origCapacity = mem.getUint32(capacityField, true);

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage({pages, capacityField});
assertEquals('ready', worker.getMessage());

(() => {
  youngHolder[0] = new Array(16370).fill(1.1);
})();
gc();
mem.setUint32(capacityField, origCapacity, true);
anchors = null;
assertEquals('done', worker.getMessage());
worker.terminate();

for (let i = 0; i < 1000; i++) {
  new Array(100).fill(i);
}
