// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --sandbox-testing --code-cache-fuzzing

// %GetCodeCache compiles and serializes a script. It returns a handle to the
// header and trusted section of the code cache, which stay outside the
// sandbox, and a copy of the untrusted section. The script can use the memory
// corruption API on the objects deserialized from the cache; here it changes
// the length of add's SharedFunctionInfo from 2 to 42.
const source = `{
  function add(a, b) { return a + b; }
  const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const kHeapObjectTag = 1;
  const sfi = memory.getUint32(
      Sandbox.getAddressOf(add) +
          Sandbox.getFieldOffset(
              Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE'),
              'shared_function_info'),
      true) - kHeapObjectTag;
  memory.setUint16(
      sfi +
          Sandbox.getFieldOffset(
              Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE'),
              'length'),
      42, true);
  add.length;
}`;
const {cache, untrusted} = %GetCodeCache(source);
assertTrue(untrusted instanceof ArrayBuffer);

// %ConsumeCodeCache deserializes the original header and trusted section
// together with any untrusted section, runs the script and returns its
// completion value.
assertEquals(42, %ConsumeCodeCache(cache, untrusted));
