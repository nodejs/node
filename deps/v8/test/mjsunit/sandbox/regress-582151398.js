// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing --code-cache-fuzzing

// Bytecodes from serializer-deserializer.h.
const kNewObjectOld = 0x01;
const kReadOnlyHeapRef = 0x05;
const kNop = 0x0a;
const kSynchronize = 0x0b;
const kVariableRawData = 0x11;

const kHeapObjectTag = 1;
const kTaggedSize = 4;

function putUint30(v) {
  v = (v >>> 0) << 2;
  const bytes = v <= 0xff ? 1 : v <= 0xffff ? 2 : v <= 0xffffff ? 3 : 4;
  v |= bytes - 1;
  return Array.from({length: bytes}, (_, i) => (v >>> (8 * i)) & 0xff);
}

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const builtinCode = Sandbox.getBuiltinCode(0);
const codeMap = memory.getUint32(builtinCode - kHeapObjectTag, true) - kHeapObjectTag;
assertTrue(codeMap < 0x40000);
const codePayloadSlots = Sandbox.getSizeOfObjectAt(builtinCode) / kTaggedSize - 1;

// Append a top-level Code object allocated in old space (inside the sandbox)
// to the untrusted section. PostProcessNewObject must reject it with
// OutsideSandboxCast<Code>.
const {cache, untrusted} = %GetCodeCache('1 + 1');
const bytes = Array.from(new Uint8Array(untrusted));
let end = bytes.length;
while (bytes[end - 1] === kNop) end--;
assertEquals(kSynchronize, bytes[end - 1]);
const malformed = new Uint8Array([
  ...bytes.slice(0, end - 1),
  kNewObjectOld,
  kReadOnlyHeapRef, ...putUint30(codeMap >>> 2),
  kVariableRawData, ...putUint30(codePayloadSlots),
  ...new Uint8Array(codePayloadSlots * kTaggedSize),
  kSynchronize, kSynchronize,
]);
%ConsumeCodeCache(cache, malformed.buffer);

assertUnreachable('Process should have been killed.');
