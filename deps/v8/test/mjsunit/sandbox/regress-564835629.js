// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --cache=after-execute

function target() {
  const arr = [1, 2, 3];
  const sandboxMemory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const arrAddress = Sandbox.getAddressOf(arr);
  const elementsAddress = sandboxMemory.getUint32(arrAddress + 8, true) - 1;
  const kSmiMinusOne = (-1 << 1) >>> 0;
  sandboxMemory.setUint32(elementsAddress + 4, kSmiMinusOne, true);
}

target();
