// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const segment = (new Intl.Segmenter).segment();

const addr = Sandbox.getAddressOf(segment);
const buffer = new DataView(new Sandbox.MemoryView(0, 0x100000000));
buffer.setUint8(addr + 22, 0x1);

gc();
new Uint8Array(segment);
