// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function TestTransferSucceeds() {
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, undefined);
  ab.transfer();
  assertEquals(0, ab.byteLength);  // Detached.
}

function TestTransferFails() {
  const ab = new ArrayBuffer(100);
  %ArrayBufferSetDetachKey(ab, Symbol());
  assertThrows(() => { ab.transfer(); }, TypeError);
  assertEquals(100, ab.byteLength);  // Not detached.
}

TestTransferSucceeds();
TestTransferFails();
