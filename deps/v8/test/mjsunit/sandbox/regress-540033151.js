// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

// Regression test for issue 540033151:
// A sandbox attacker copies an ArrayBufferExtension handle (tagged with
// kArrayBufferExtensionTag) into an external pointer slot expecting a
// different tag (e.g. Foreign::foreign_address_ with
// kAnyForeignExternalPointerTagRange).
// Marking and evacuation in the ExternalPointerTable must validate that the
// entry's tag matches slot.tag_range(), safely trapping any mismatch via
// SBXCHECK.

let ab = new ArrayBuffer(64);
let abTypeId = Sandbox.getInstanceTypeIdOf(ab);
let extOffset = Sandbox.getFieldOffset(abTypeId, "extension");
let mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let extHandle = mem.getUint32(Sandbox.getAddressOf(ab) + extOffset, true);

let marker = {};
let p = import("data:text/javascript,export const a = 1;");
let markerAddr = Sandbox.getAddressOf(marker);
let foreignAddr = 0;
for (let i = 0; i < 4000; i += 4) {
  let a = (markerAddr + i) >>> 0;
  if (Sandbox.isValidObjectAt(a)) {
    if (Sandbox.getInstanceTypeOfObjectAt(a) === "FOREIGN_TYPE") {
      foreignAddr = a;
      break;
    }
  }
}
assertTrue(foreignAddr !== 0);

let foreignOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor("FOREIGN_TYPE"), "foreign_address");

// Overwrite Foreign's foreign_address with ArrayBuffer's extension handle.
mem.setUint32(foreignAddr + foreignOffset, extHandle, true);

globalThis.ab = ab;
globalThis.p = p;

// Major GC visits Foreign and attempts to mark the EPT entry with
// kAnyForeignExternalPointerTagRange. The mismatch against
// kArrayBufferExtensionTag must trigger an SBXCHECK trap.
gc({type: "major"});

assertUnreachable("Cross-tag external pointer table aliasing should be trapped by SBXCHECK");
