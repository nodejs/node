// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing --code-cache-fuzzing

// Bytecodes from serializer-deserializer.h.
const kNewObjectOld = 0x01;
const kRootArrayConstants = 0x40;
const kRootArrayConstantsCount = 0x20;

// Keep only the start of the first object of the untrusted section, the
// SharedFunctionInfo: kNewObject and its map. The deserializer then reads the
// object's fields from the padding, which is all kNop, and past the end of the
// code cache into the guard page behind it. The guard page is safe to read, so
// the read must kill the process without a sandbox violation.
const {cache, untrusted} = %GetCodeCache('1 + 1');
const bytes = new Uint8Array(untrusted);
assertEquals(kNewObjectOld, bytes[0]);
assertTrue(bytes[1] >= kRootArrayConstants &&
           bytes[1] < kRootArrayConstants + kRootArrayConstantsCount);
%ConsumeCodeCache(cache, bytes.slice(0, 2).buffer);

assertUnreachable('Process should have been killed.');
