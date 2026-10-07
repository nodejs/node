// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let kMax31BitSmi = (1 << 30) - 1;
let k1MiB = 1 * 1024 * 1024;
let k1GiB = 1 * 1024 * 1024 * 1024;
let k4GiB = 4 * k1GiB;
let kMaxBufferSize = 2 * k1GiB - kPageSize; // TODO(titzer): raise this to 4GiB
let kStrideLength = 65536;

(function Test() {
  if (typeof WebAssembly === 'undefined') return;
  var buffer;
  try {
    let kPages = kMaxBufferSize / kPageSize;
    let memory = new WebAssembly.Memory({initial: kPages, maximum: kPages});
    buffer = memory.buffer;
  } catch (e) {
    print("OOM: sorry, best effort max array buffer size test!");
    return;
  }

  print("Allocated " + buffer.byteLength + " bytes");
  assertEquals(kMaxBufferSize, buffer.byteLength);

  function probe(view, stride, f) {
    print("--------------------");
    let max = view.length;
    for (let i = 0; i < max; i += stride) {
      view[i] = f(i);
    }
    for (let i = 0; i < max; i += stride) {
      //    print(`${i} = ${f(i)}`);
      assertEquals(f(i), view[i]);
    }
  }

  {
    // Make an uint32 view and probe it.
    let elemSize = 4;
    let viewSize = kMaxBufferSize / elemSize;
    // TODO(titzer): view sizes are limited to 31 bit SMIs. fix.
    if (viewSize <= kMax31BitSmi) {
      let uint32 = new Uint32Array(buffer);
      assertEquals(kMaxBufferSize / elemSize, uint32.length);
      probe(uint32, kStrideLength / elemSize,
            i => (0xaabbccee ^ ((i >> 11) * 0x110005)) >>> 0);
    }
  }

  {
    // Make an uint16 view and probe it.
    let elemSize = 2;
    let viewSize = kMaxBufferSize / elemSize;
    // TODO(titzer): view sizes are limited to 31 bit SMIs. fix.
    if (viewSize <= kMax31BitSmi) {
      let uint16 = new Uint16Array(buffer);
      assertEquals(kMaxBufferSize / elemSize, uint16.length);
      probe(uint16, kStrideLength / elemSize,
            i => (0xccee ^ ((i >> 11) * 0x110005)) & 0xFFFF);
    }
  }

  {
    // Make an uint8 view and probe it.
    let elemSize = 1;
    let viewSize = kMaxBufferSize / elemSize;
    // TODO(titzer): view sizes are limited to 31 bit SMIs. fix.
    if (viewSize <= kMax31BitSmi) {
      let uint8 = new Uint8Array(buffer);
      assertEquals(kMaxBufferSize / elemSize, uint8.length);
      probe(uint8, kStrideLength / elemSize,
            i => (0xee ^ ((i >> 11) * 0x05)) & 0xFF);
    }
  }

  {
    // Make a float64 view and probe it.
    let elemSize = 8;
    let viewSize = kMaxBufferSize / elemSize;
    // TODO(titzer): view sizes are limited to 31 bit SMIs. fix.
    if (viewSize <= kMax31BitSmi) {
      let float64 = new Float64Array(buffer);
      assertEquals(kMaxBufferSize / elemSize, float64.length);
      probe(float64, kStrideLength / elemSize,
            i => 0xaabbccee ^ ((i >> 11) * 0x110005));
    }
  }

  {
    // Make a float32 view and probe it.
    let elemSize = 4;
    let viewSize = kMaxBufferSize / elemSize;
    // TODO(titzer): view sizes are limited to 31 bit SMIs. fix.
    if (viewSize <= kMax31BitSmi) {
      let float32 = new Float32Array(buffer);
      assertEquals(kMaxBufferSize / elemSize, float32.length);
      probe(float32, kStrideLength / elemSize,
            i => Math.fround(0xaabbccee ^ ((i >> 11) * 0x110005)));
    }
  }
})();
