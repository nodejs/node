// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing --code-cache-fuzzing

// Bytecodes from serializer-deserializer.h.
const kRootArray = 0x07;
const kNop = 0x0a;
const kSynchronize = 0x0b;
// 0x3fffffff, encoded like SnapshotByteSink::PutUint30.
const kOutOfBoundsRootIndex = [0xff, 0xff, 0xff, 0xff];

// Append one more top-level object to the untrusted section: a root with an
// out-of-bounds index. The deserializer must stop on its bounds check.
const {cache, untrusted} = %GetCodeCache('1 + 1');
const bytes = Array.from(new Uint8Array(untrusted));
let end = bytes.length;
while (bytes[end - 1] === kNop) end--;
assertEquals(kSynchronize, bytes[end - 1]);
const malformed = new Uint8Array([
  ...bytes.slice(0, end - 1), kRootArray, ...kOutOfBoundsRootIndex,
  kSynchronize, kSynchronize
]);
%ConsumeCodeCache(cache, malformed.buffer);

assertUnreachable('Process should have been killed.');
