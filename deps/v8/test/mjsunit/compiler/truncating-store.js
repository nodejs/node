// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var asm = (function Module(global, env, buffer) {
    "use asm";

    var i8 = new global.Int8Array(buffer);
    var u8 = new global.Uint8Array(buffer);
    var i16 = new global.Int16Array(buffer);
    var u16 = new global.Uint16Array(buffer);
    var i32 = new global.Int32Array(buffer);
    var u32 = new global.Uint32Array(buffer);

    var H = 0;

    function store_i8() {
      H = 4294967295;
      i8[0 >> 0]= H;
      return i8[0 >> 0];
    }

    function store_u8() {
      H = 4294967295;
      u8[0 >> 0]= H;
      return u8[0 >> 0];
    }

    function store_i16() {
      H = 4294967295;
      i16[0 >> 0]= H;
      return i16[0 >> 0];
    }

    function store_u16() {
      H = 4294967295;
      u16[0 >> 0]= H;
      return u16[0 >> 0];
    }

    function store_i32() {
      H = 4294967295;
      i32[0 >> 0]= H;
      return i32[0 >> 0];
    }

    function store_u32() {
      H = 4294967295;
      u32[0 >> 0]= H;
      return u32[0 >> 0];
    }

    return { store_i8: store_i8,
             store_u8: store_u8,
             store_i16: store_i16,
             store_u16: store_u16,
             store_i32: store_i32,
             store_u32: store_u32 };
  })({
    "Int8Array": Int8Array,
    "Uint8Array": Uint8Array,
    "Int16Array": Int16Array,
    "Uint16Array": Uint16Array,
    "Int32Array": Int32Array,
    "Uint32Array": Uint32Array
  }, {}, new ArrayBuffer(64 * 1024));

  assertEquals(-1, asm.store_i8());
  assertEquals(255, asm.store_u8());
  assertEquals(-1, asm.store_i16());
  assertEquals(65535, asm.store_u16());
  assertEquals(-1, asm.store_i32());
  assertEquals(4294967295, asm.store_u32());
})();

(function() {
  var asm = (function Module(global, env, buffer) {
    "use asm";

    var i32 = new global.Int32Array(buffer);

    var H = 0;

    // This is not valid asm.js, but we should still generate correct code.
    function store_i32_from_string() {
      H = "3";
      i32[0 >> 0]= H;
      return i32[0 >> 0];
    }

    return { store_i32_from_string: store_i32_from_string };
  })({
    "Int32Array": Int32Array
  }, {}, new ArrayBuffer(64 * 1024));

  assertEquals(3, asm.store_i32_from_string());
})();
