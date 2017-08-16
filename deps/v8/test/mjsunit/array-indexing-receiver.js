// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

// Ensure `Array.prototype.indexOf` functions correctly for numerous elements
// kinds, and various exotic receiver types,

var kIterCount = 1;
var kTests = {
  Array: {
    FAST_ELEMENTS() {
      var r = /foo/;
      var s = new String("bar");
      var p = new Proxy({}, {});
      var o = {};

      var array = [r, s, p];
      assertTrue(%HasFastObjectElements(array));
      assertFalse(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(p), 2);
        assertEquals(array.indexOf(o), -1);
      }
    },

    FAST_HOLEY_ELEMENTS() {
      var r = /foo/;
      var p = new Proxy({}, {});
      var o = {};

      var array = [r, , p];
      assertTrue(%HasFastObjectElements(array));
      assertTrue(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(p), 2);
        assertEquals(array.indexOf(o), -1);
      }
    },

    FAST_SMI_ELEMENTS() {
      var array = [0, 88, 9999, 1, -5, 7];
      assertTrue(%HasFastSmiElements(array));
      assertFalse(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(9999), 2);
        assertEquals(array.indexOf(-5), 4);
        assertEquals(array.indexOf(-5.00001), -1);
        assertEquals(array.indexOf(undefined), -1);
        assertEquals(array.indexOf(NaN), -1);
      }
    },

    FAST_HOLEY_SMI_ELEMENTS() {
      var array = [49, , , 72, , , 67, -48];
      assertTrue(%HasFastSmiElements(array));
      assertTrue(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(72), 3);
        assertEquals(array.indexOf(-48), 7);
        assertEquals(array.indexOf(72, 4), -1);
        assertEquals(array.indexOf(undefined), -1);
        assertEquals(array.indexOf(undefined, -2), -1);
        assertEquals(array.indexOf(NaN), -1);
      }
    },

    FAST_DOUBLE_ELEMENTS() {
      var array = [7.00000001, -13000.89412, 73451.4124,
                   5824.48, 6.0000495, 48.3488, 44.0, 76.35, NaN, 78.4];
      assertTrue(%HasFastDoubleElements(array));
      assertFalse(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(7.00000001), 0);
        assertEquals(array.indexOf(7.00000001, 2), -1);
        assertEquals(array.indexOf(NaN), -1);
        assertEquals(array.indexOf(NaN, -1), -1);
        assertEquals(array.indexOf(-13000.89412), 1);
        assertEquals(array.indexOf(-13000.89412, -2), -1);
        assertEquals(array.indexOf(undefined), -1);
      }
    },

    FAST_HOLEY_DOUBLE_ELEMENTS() {
      var array = [7.00000001, -13000.89412, ,
                   5824.48, , 48.3488, , NaN, , 78.4];
      assertTrue(%HasFastDoubleElements(array));
      assertTrue(%HasFastHoleyElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(7.00000001), 0);
        assertEquals(array.indexOf(7.00000001, 2), -1);
        assertEquals(array.indexOf(NaN), -1);
        assertEquals(array.indexOf(NaN, -2), -1);
        assertEquals(array.indexOf(-13000.89412), 1);
        assertEquals(array.indexOf(-13000.89412, -2), -1);
        assertEquals(array.indexOf(undefined, -2), -1);
        assertEquals(array.indexOf(undefined, -1), -1);
      }
    },

    DICTIONARY_ELEMENTS() {
      var array = [];
      Object.defineProperty(array, 4, { get() { gc(); return NaN; } });
      Object.defineProperty(array, 7, { value: Function });

      assertTrue(%HasDictionaryElements(array));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(array.indexOf(NaN), -1);
        assertEquals(array.indexOf(NaN, -3), -1);
        assertEquals(array.indexOf(Function), 7);
        assertEquals(array.indexOf(undefined), -1);
        assertEquals(array.indexOf(undefined, 7), -1);
      }
    },
  },

  Object: {
    FAST_ELEMENTS() {
      var r = /foo/;
      var s = new String("bar");
      var p = new Proxy({}, {});
      var o = {};

      var object = { 0: r, 1: s, 2: p, length: 3 };
      assertTrue(%HasFastObjectElements(object));
      // TODO(caitp): JSObjects always seem to start with FAST_HOLEY_ELEMENTS
      // assertFalse(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, p), 2);
        assertEquals(Array.prototype.indexOf.call(object, o), -1);
      }
    },

    FAST_HOLEY_ELEMENTS() {
      var r = /foo/;
      var p = new Proxy({}, {});
      var o = {};

      var object = { 0: r, 2: p, length: 3 };
      assertTrue(%HasFastObjectElements(object));
      assertTrue(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, p), 2);
        assertEquals(Array.prototype.indexOf.call(object, o), -1);
      }
    },

    FAST_SMI_ELEMENTS() {
      var object = { 0: 0, 1: 88, 2: 9999, 3: 1, 4: -5, 5: 7, length: 6 };
      // TODO(caitp): JSObjects always seem to start with FAST_HOLEY_ELEMENTS
      // assertTrue(%HasFastSmiElements(object));
      // assertFalse(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, 9999), 2);
        assertEquals(Array.prototype.indexOf.call(object, -5), 4);
        assertEquals(Array.prototype.indexOf.call(object, -5.00001), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN), -1);
      }
    },

    FAST_HOLEY_SMI_ELEMENTS() {
      var object = { 0: 49, 3: 72, 6: 67, 7: -48, length: 8 };
      // TODO(caitp): JSObjects always seem to start with FAST_HOLEY_ELEMENTS
      // assertTrue(%HasFastSmiElements(object));
      // assertTrue(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, 72), 3);
        assertEquals(Array.prototype.indexOf.call(object, -48), 7);
        assertEquals(Array.prototype.indexOf.call(object, 72, 4), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined, -2), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN), -1);
      }
    },

    FAST_DOUBLE_ELEMENTS() {
      var object = { 0: 7.00000001, 1: -13000.89412, 2: 73451.4124,
                   3: 5824.48, 4: 6.0000495, 5: 48.3488, 6: 44.0, 7: 76.35,
                   8: NaN, 9: 78.4, length: 10 };
      // TODO(caitp): JSObjects always seem to start with FAST_HOLEY_ELEMENTS
      // assertTrue(%HasFastDoubleElements(object));
      // assertFalse(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, 7.00000001), 0);
        assertEquals(Array.prototype.indexOf.call(object, 7.00000001, 2), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN, -1), -1);
        assertEquals(Array.prototype.indexOf.call(object, -13000.89412), 1);
        assertEquals(Array.prototype.indexOf.call(object, -13000.89412, -2), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined), -1);
      }
    },

    FAST_HOLEY_DOUBLE_ELEMENTS() {
      var object = { 0: 7.00000001, 1: -13000.89412, 3: 5824.48, 5: 48.3488,
                    7: NaN, 9: 78.4, length: 10 };
      // TODO(caitp): JSObjects always seem to start with FAST_HOLEY_ELEMENTS
      // assertTrue(%HasFastDoubleElements(object));
      // assertTrue(%HasFastHoleyElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, 7.00000001), 0);
        assertEquals(Array.prototype.indexOf.call(object, 7.00000001, 2), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN, -2), -1);
        assertEquals(Array.prototype.indexOf.call(object, -13000.89412), 1);
        assertEquals(Array.prototype.indexOf.call(object, -13000.89412, -2), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined, -2), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined, -1), -1);
      }
    },

    DICTIONARY_ELEMENTS() {
      var object = { length: 8 };
      Object.defineProperty(object, 4, { get() { gc(); return NaN; } });
      Object.defineProperty(object, 7, { value: Function });

      assertTrue(%HasDictionaryElements(object));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(object, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(object, NaN, -3), -1);
        assertEquals(Array.prototype.indexOf.call(object, Function), 7);
        assertEquals(Array.prototype.indexOf.call(object, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(object, undefined, 7), -1);
      }

      (function prototypeModifiedDuringAccessor() {
        function O() {
          return {
            __proto__: {},
            get 0() {
              gc();
              this.__proto__.__proto__ = {
                get 1() {
                  gc();
                  this[2] = "c";
                  return "b";
                }
              };
              return "a";
            },
            length: 3
          };
        }

        // Switch to slow path when first accessor modifies the prototype
        assertEquals(Array.prototype.indexOf.call(O(), "a"), 0);
        assertEquals(Array.prototype.indexOf.call(O(), "b"), 1);
        assertEquals(Array.prototype.indexOf.call(O(), "c"), 2);

        // Avoid switching to slow path due to avoiding the accessor
        assertEquals(Array.prototype.indexOf.call(O(), "c", 2), -1);
        assertEquals(Array.prototype.indexOf.call(O(), "b", 1), -1);
        assertEquals(Array.prototype.indexOf.call(O(), undefined, 1), 1);
      });
    },
  },

  String: {
    FAST_STRING_ELEMENTS() {
      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call("froyo", "y"), 3);
        assertEquals(Array.prototype.indexOf.call("froyo", "y", -1), -1);
        assertEquals(Array.prototype.indexOf.call("froyo", "y", -2), 3);
        assertEquals(Array.prototype.indexOf.call("froyo", NaN), -1);
        assertEquals(Array.prototype.indexOf.call("froyo", undefined), -1);
      }
    },

    SLOW_STRING_ELEMENTS() {
      var string = new String("froyo");

      // Never accessible from A.p.indexOf as 'length' is not configurable
      Object.defineProperty(string, 34, { value: NaN });
      Object.defineProperty(string, 12, { get() { return "nope" } });

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call("froyo", "y"), 3);
        assertEquals(Array.prototype.indexOf.call("froyo", "y", -1), -1);
        assertEquals(Array.prototype.indexOf.call("froyo", "y", -2), 3);
        assertEquals(Array.prototype.indexOf.call(string, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(string, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(string, "nope"), -1);
      }
    },
  },

  Arguments: {
    FAST_SLOPPY_ARGUMENTS_ELEMENTS() {
      var args = (function(a, b) { return arguments; })("foo", NaN, "bar");
      assertTrue(%HasSloppyArgumentsElements(args));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(args, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(args, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(args, NaN, -1), -1);
        assertEquals(Array.prototype.indexOf.call(args, "bar", -1), 2);
      }
    },

    SLOW_SLOPPY_ARGUMENTS_ELEMENTS() {
      var args = (function(a, a) { return arguments; })("foo", NaN, "bar");
      Object.defineProperty(args, 3, { get() { gc(); return "silver"; } });
      Object.defineProperty(args, "length", { value: 4 });
      assertTrue(%HasSloppyArgumentsElements(args));

      for (var i = 0; i < kIterCount; ++i) {
        assertEquals(Array.prototype.indexOf.call(args, undefined), -1);
        assertEquals(Array.prototype.indexOf.call(args, NaN), -1);
        assertEquals(Array.prototype.indexOf.call(args, NaN, -2), -1);
        assertEquals(Array.prototype.indexOf.call(args, "bar", -2), 2);
        assertEquals(Array.prototype.indexOf.call(args, "silver", -1), 3);
      }
    }
  },

  TypedArray: {
    Int8Array() {
      var array = new Int8Array([-129, 128,
                                 NaN /* 0 */, +0 /* 0 */, -0 /* 0 */,
                                 +Infinity /* 0 */, -Infinity /* 0 */,
                                 255 /* -1 */, 127 /* 127 */, -255 /* 1 */]);
      assertEquals(Array.prototype.indexOf.call(array, -129), -1);
      assertEquals(Array.prototype.indexOf.call(array, 128), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, -1, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, -1, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 127, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 127, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 1, 9), 9);
    },

    Detached_Int8Array() {
      var array = new Int8Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Uint8Array() {
      var array = new Uint8Array([-1, 256,
                                  NaN /* 0 */, +0 /* 0 */, -0 /* 0 */,
                                  +Infinity /* 0 */, -Infinity /* 0 */,
                                  255 /* 255 */, 257 /* 1 */, -128 /* 128 */,
                                  -2 /* 254 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1), -1);
      assertEquals(Array.prototype.indexOf.call(array, 256), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, 255, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, 255, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 1, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 1, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 128, 9), 9);
      assertEquals(Array.prototype.indexOf.call(array, 128, 10), -1);

      assertEquals(Array.prototype.indexOf.call(array, 254, 10), 10);
    },

    Detached_Uint8Array() {
      var array = new Uint8Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Uint8ClampedArray() {
      var array = new Uint8ClampedArray([-1 /* 0 */, NaN /* 0 */, 256 /* 255 */,
                                         127.6 /* 128 */, 127.4 /* 127 */,
                                         121.5 /* 122 */, 124.5 /* 124 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1), -1);
      assertEquals(Array.prototype.indexOf.call(array, 256), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0), 0);
      assertEquals(Array.prototype.indexOf.call(array, 0, 1), 1);
      assertEquals(Array.prototype.indexOf.call(array, 255, 2), 2);

      assertEquals(Array.prototype.indexOf.call(array, 128, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 128, 4), -1);

      assertEquals(Array.prototype.indexOf.call(array, 127, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 127, 5), -1);

      assertEquals(Array.prototype.indexOf.call(array, 122, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 122, 6), -1);

      assertEquals(Array.prototype.indexOf.call(array, 124, 6), 6);
    },

    Detached_Uint8ClampedArray() {
      var array = new Uint8ClampedArray(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Int16Array() {
      var array = new Int16Array([-32769, 32768,
                                  NaN /* 0 */, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFF /* -1 */, 30000 /* 30000 */,
                                  300000 /* -27680 */]);
      assertEquals(Array.prototype.indexOf.call(array, -32769), -1);
      assertEquals(Array.prototype.indexOf.call(array, 32768), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, -1, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, -1, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 30000, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 30000, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, -27680, 9), 9);
    },

    Detached_Int16Array() {
      var array = new Int16Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Uint16Array() {
      var array = new Uint16Array([-1, 65536,
                                  NaN /* 0 */, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFF /* 65535 */, 300000 /* 37856 */,
                                  3000000 /* 50880 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1), -1);
      assertEquals(Array.prototype.indexOf.call(array, 65536), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, 65535, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, 65535, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 37856, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 37856, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 50880, 9), 9);
    },

    Detached_Uint16Array() {
      var array = new Uint16Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Int32Array() {
      var array = new Int32Array([-2147483649, 2147483648,
                                  NaN /* 0 */, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFFFFFF /* -1 */, 4294968064 /* 768 */,
                                  4294959447 /* -7849 */]);
      assertEquals(Array.prototype.indexOf.call(array, -2147483649), -1);
      assertEquals(Array.prototype.indexOf.call(array, 2147483648), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0.0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, -1, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, -1, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 768, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 768, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, -7849, 9), 9);
    },

    Detached_Int32Array() {
      var array = new Int32Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Uint32Array() {
      var array = new Uint32Array([-1, 4294967296,
                                  NaN /* 0 */, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFFFFFF /* 4294967295 */,
                                  4294968064 /* 768 */,
                                  4295079447 /* 112151 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1), -1);
      assertEquals(Array.prototype.indexOf.call(array, 4294967296), -1);

      assertEquals(Array.prototype.indexOf.call(array, 0.0, 2), 2);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, 0, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, 4294967295, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, 4294967295, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, 768, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, 768, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 112151, 9), 9);
    },

    Detached_Uint32Array() {
      var array = new Uint32Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Float32Array() {
      var array = new Float32Array([-1, 4294967296,
                                  NaN, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFFFFFF /* 34359738368.0 */,
                                  -4294968064 /* -4294968320.0 */,
                                  4295079447 /* 4295079424.0 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1.0), 0);
      assertEquals(Array.prototype.indexOf.call(array, 4294967296), 1);

      assertEquals(Array.prototype.indexOf.call(array, NaN, 2), -1);
      assertEquals(Array.prototype.indexOf.call(array, Infinity, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, -Infinity, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, 34359738368.0, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, 34359738368.0, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, -4294968320.0, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, -4294968320.0, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 4295079424.0, 9), 9);
    },

    Detached_Float32Array() {
      var array = new Float32Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },

    Float64Array() {
      var array = new Float64Array([-1, 4294967296,
                                  NaN, Infinity /* 0 */,
                                  -Infinity /* 0 */, -0 /* 0 */, +0 /* 0 */,
                                  0x7FFFFFFFF /* 34359738367.0 */,
                                  -4294968064 /* -4294968064.0 */,
                                  4295079447 /* 4295079447.0 */]);
      assertEquals(Array.prototype.indexOf.call(array, -1.0), 0);
      assertEquals(Array.prototype.indexOf.call(array, 4294967296), 1);

      assertEquals(Array.prototype.indexOf.call(array, NaN, 2), -1);
      assertEquals(Array.prototype.indexOf.call(array, Infinity, 3), 3);
      assertEquals(Array.prototype.indexOf.call(array, -Infinity, 4), 4);
      assertEquals(Array.prototype.indexOf.call(array, 0, 5), 5);
      assertEquals(Array.prototype.indexOf.call(array, 0, 6), 6);
      assertEquals(Array.prototype.indexOf.call(array, 0.0, 7), -1);

      assertEquals(Array.prototype.indexOf.call(array, 34359738367.0, 7), 7);
      assertEquals(Array.prototype.indexOf.call(array, 34359738367.0, 8), -1);

      assertEquals(Array.prototype.indexOf.call(array, -4294968064.0, 8), 8);
      assertEquals(Array.prototype.indexOf.call(array, -4294968064.0, 9), -1);

      assertEquals(Array.prototype.indexOf.call(array, 4295079447.0, 9), 9);
    },

    Detached_Float64Array() {
      var array = new Float32Array(10);
      %ArrayBufferNeuter(array.buffer);
      assertEquals(Array.prototype.indexOf.call(array, 0), -1);
      assertEquals(Array.prototype.indexOf.call(array, 0, 10), -1);
    },
  }
};

function runSuites(suites) {
  Object.keys(suites).forEach(suite => runSuite(suites[suite]));

  function runSuite(suite) {
    Object.keys(suite).forEach(test => suite[test]());
  }
}

runSuites(kTests);
