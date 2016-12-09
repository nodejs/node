// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

const stdlib = {
  Math: Math,
  Int8Array: Int8Array,
  Int16Array: Int16Array,
  Int32Array: Int32Array,
  Uint8Array: Uint8Array,
  Uint16Array: Uint16Array,
  Uint32Array: Uint32Array,
  Float32Array: Float32Array,
  Float64Array: Float64Array,
};

const buffer = new ArrayBuffer(65536);
const BASE = 1000000000;

const OOB_INDEXES = [
  buffer.byteLength,
  buffer.byteLength + 1,
  buffer.byteLength + 2,
  buffer.byteLength + 3,
  buffer.byteLength + 4,
  buffer.byteLength + 5,
  buffer.byteLength + 6,
  buffer.byteLength + 7,
  buffer.byteLength + 8,
  buffer.byteLength + 9,
  buffer.byteLength + 10,
  0x80000000,
  0x80000004,
  0xF0000000,
  0xFFFFFFFF,
  0xFFFFFFFE,
  -1, -2, -3, -4, -5, -6, -7, -8
];

function resetBuffer() {
  var view = new Int32Array(buffer);
  for (var i = 0; i < view.length; i++) {
    view[i] = BASE | (i << 2);
  }
}
resetBuffer();


function checkView(view, load, shift) {
  for (var i = 0; i < 300; i++) {
    assertEquals(view[i >> shift], load(i));
  }
}

function RunAsmJsTest(asmfunc, expect) {
  var asm_source = asmfunc.toString();
  var nonasm_source = asm_source.replace(new RegExp("use asm"), "");

  print("Testing " + asmfunc.name + " (js)...");
  var js_module = eval("(" + nonasm_source + ")")(stdlib, {}, buffer);
  expect(js_module);

  print("Testing " + asmfunc.name + " (asm.js)...");
  var asm_module = asmfunc(stdlib, {}, buffer);
  assertTrue(%IsAsmWasmCode(asmfunc));
  expect(asm_module);
}

function LoadAt_i32(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(buffer);
  function load(a) {
    a = a | 0;
    return HEAP32[a >> 2] | 0;
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_i32, function(module) {
  var load = module.load;
  assertEquals(BASE, load(0));
  assertEquals(BASE | 0x30, load(0x30));
  assertEquals(BASE | 0x704, load(0x704));
  assertEquals(BASE | 0x704, load(0x705));
  assertEquals(BASE | 0x704, load(0x706));
  assertEquals(BASE | 0x704, load(0x707));

  var length = buffer.byteLength;
  assertEquals(BASE | (length - 4), load(length - 4));
  assertEquals(BASE | (length - 4), load(length - 4 + 1));
  assertEquals(BASE | (length - 4), load(length - 4 + 2));
  assertEquals(BASE | (length - 4), load(length - 4 + 3));

  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Int32Array(buffer), load, 2);
});

function LoadAt_i16(stdlib, foreign, buffer) {
  "use asm";
  var HEAP16 = new stdlib.Int16Array(buffer);
  function load(a) {
    a = a | 0;
    return HEAP16[a >> 1] | 0;
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_i16, function(module) {
  var load = module.load;
  var LOWER = (BASE << 16) >> 16;
  var UPPER = BASE >> 16;
  assertEquals(LOWER, load(0));
  assertEquals(UPPER, load(2));

  assertEquals(LOWER | 0x30, load(0x30));
  assertEquals(UPPER, load(0x32));

  assertEquals(LOWER | 0x504, load(0x504));
  assertEquals(LOWER | 0x504, load(0x505));

  assertEquals(UPPER, load(0x706));
  assertEquals(UPPER, load(0x707));

  var length = buffer.byteLength;
  assertEquals(LOWER | (length - 4), load(length - 4));
  assertEquals(LOWER | (length - 4), load(length - 4 + 1));
  assertEquals(UPPER, load(length - 4 + 2));
  assertEquals(UPPER, load(length - 4 + 3));

  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Int16Array(buffer), load, 1);
});

function LoadAt_u16(stdlib, foreign, buffer) {
  "use asm";
  var HEAP16 = new stdlib.Uint16Array(buffer);
  function load(a) {
    a = a | 0;
    return HEAP16[a >> 1] | 0;
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_u16, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Uint16Array(buffer), load, 1);
});

function LoadAt_i8(stdlib, foreign, buffer) {
  "use asm";
  var HEAP8 = new stdlib.Int8Array(buffer);
  function load(a) {
    a = a | 0;
    return HEAP8[a >> 0] | 0;
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_i8, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Int8Array(buffer), load, 0);
});

function LoadAt_u8(stdlib, foreign, buffer) {
  "use asm";
  var HEAP8 = new stdlib.Uint8Array(buffer);
  function load(a) {
    a = a | 0;
    return HEAP8[a >> 0] | 0;
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_u8, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Uint8Array(buffer), load, 0);
});


function LoadAt_u32(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Uint32Array(buffer);
  function load(a) {
    a = a | 0;
    return +(HEAP32[a >> 2] >>> 0);
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_u32, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(0, load(index));
  checkView(new Uint32Array(buffer), load, 2);
});

function LoadAt_f32(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Float32Array(buffer);
  var fround = stdlib.Math.fround;
  function load(a) {
    a = a | 0;
    return fround(HEAP32[a >> 2]);
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_f32, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(NaN, load(index));
  checkView(new Float32Array(buffer), load, 2);
});

function LoadAt_f64(stdlib, foreign, buffer) {
  "use asm";
  var HEAP64 = new stdlib.Float64Array(buffer);
  function load(a) {
    a = a | 0;
    return +HEAP64[a >> 3];
  }
  return {load: load};
}

RunAsmJsTest(LoadAt_f64, function(module) {
  var load = module.load;
  for (index of OOB_INDEXES) assertEquals(NaN, load(index));
  checkView(new Float64Array(buffer), load, 3);
});

// TODO(titzer): constant heap indexes
// TODO(titzer): heap accesses with offsets and arithmetic
// TODO(titzer): [i >> K] where K is greater than log(size)
