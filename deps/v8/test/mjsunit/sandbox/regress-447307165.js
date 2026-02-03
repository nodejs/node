// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct --sandbox-testing

function findForeign(start_addr, max_objects = 35) {
  const tasks = [];
  let addr = start_addr;
  for (let i = 0; i < max_objects; i++) {
    let size = Sandbox.getSizeOfObjectAt(addr);
    objtype = Sandbox.getInstanceTypeOfObjectAt(addr)
    if (objtype == "PROMISE_REACTION_TYPE") {
      reject_handler = r32(addr + 12);
      context = r32(reject_handler - 1 + 20);
      foreign = r32(context - 1 + 24);
      tasks.push(context - 1);
    }
    addr = (addr + size) >>> 0;
  }
  return tasks;
}

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const r32 = (addr) => mem.getUint32(addr, true);
const w32 = (addr, val) => mem.setUint32(addr, val, true);

oo = {};
(async () => {
  const m = new Atomics.Mutex();
  const p1 = Atomics.Mutex.lockAsync(m, () => 1);
  const p2 = Atomics.Mutex.lockAsync(m, () => 2);

  tasks = findForeign(Sandbox.getAddressOf(oo));
  w32(tasks[0] + 24, r32(tasks[2] + 24));

  await Promise.allSettled([p1, p2]);
})();
