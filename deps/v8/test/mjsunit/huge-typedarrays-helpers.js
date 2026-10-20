// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kB = 1024;
const MB = 1024 * kB;
const GB = 1024 * MB;

const kMaxArrayBufferByteLength = %ArrayBufferMaxByteLength();

const kHasWasm = (typeof WebAssembly != "undefined");
function makeWasmMemory(length) {
  const kWasmPageSize = 64 * 1024;
  assertTrue(kHasWasm);
  assertEquals(0, length % kWasmPageSize);
  let num_pages = BigInt(length / kWasmPageSize);
  let wasm_mem = new WebAssembly.Memory(
      {initial: num_pages, maximum: num_pages, address: 'i64'});
  return wasm_mem.buffer;
}

const kTestConfigs = [
  // 2GB+1 should flush out bugs where the length is capped to 31 bits
  // (resulting in length 1).
  [Int8Array, num_elems => new Int8Array(num_elems), 2 * GB + 1],
  // 4GB+1 should flush out bugs where the length is capped to 32 bits
  // (resulting in length 1).
  [Int8Array, num_elems => new Int8Array(num_elems), 4 * GB + 1],
  // Test the maximum array buffer length.
  [Int8Array, num_elems => new Int8Array(num_elems), kMaxArrayBufferByteLength],
  // And then test some huge arrays of different type.
  [Uint8Array, num_elems => new Uint8Array(num_elems), 21 * GB],
  [
    Int16Array, num_elems => new Uint16Array(new ArrayBuffer(num_elems * 2)),
    21 * GB
  ],
  [Int32Array, num_elems => new Int32Array(num_elems), 21 * GB]
].concat(kHasWasm ? [
  // Test a large ArrayBuffer constructed from Wasm.
  [Int8Array, num_elems => new Int8Array(makeWasmMemory(num_elems)), 16*GB]
] : []);

function isOom(e) {
  return (e instanceof RangeError) &&
          (e.message.includes('Out of memory') ||
           e.message.includes('could not allocate memory') ||
           e.message.includes('Array buffer allocation failed'));
}

function ignoreOOM(fn) {
  try {
    return fn();
  } catch (e) {
    if (isOom(e)) {
      print('Caught OOM: ' + e.message);
      return undefined;
    }
    throw e;
  }
}

function* GetTestConfigs() {
  for (let [type, constructor, length] of kTestConfigs) {
    // Ignore configuration that cannot work on this platform.
    if (length > kMaxArrayBufferByteLength) continue;
    print(`- ${type.name} of ${length} bytes`);
    let elem_size = type.BYTES_PER_ELEMENT;
    let num_elems = length / elem_size;
    let arr = ignoreOOM(() => constructor(num_elems));
    if (!arr) continue;
    assertEquals(length, arr.byteLength);
    assertEquals(num_elems, arr.length);
    assertEquals(0, arr.byteOffset);
    yield {
      type: type,
      length: length,
      arr: arr,
      elem_size: elem_size,
      num_elems: num_elems
    };
  }
}
