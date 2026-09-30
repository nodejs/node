// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const rab = new ArrayBuffer(0x1000, { maxByteLength: 0x100000 });
const sab = new SharedArrayBuffer(0x10000);

const extOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdOf(rab), 'extension');
const sabExtHandle = Sandbox.readObjectField(sab, extOffset, 32);

Sandbox.corruptObjectField(rab, extOffset, sabExtHandle, 32);
rab.resize(0x1000);
