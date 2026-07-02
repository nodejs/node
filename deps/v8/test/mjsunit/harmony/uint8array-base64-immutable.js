// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64 --js-immutable-arraybuffer

const ab = new ArrayBuffer(10);
const immutable_ab = ab.transferToImmutable();
const uint8array = new Uint8Array(immutable_ab);

assertThrows(() => uint8array.setFromBase64("Zm9vYmFy"), TypeError);
assertThrows(() => uint8array.setFromHex("666f6f626172"), TypeError);
