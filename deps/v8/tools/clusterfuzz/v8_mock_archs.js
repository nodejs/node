// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is intended for permanent JS behavior changes for mocking out
// non-deterministic behavior. For temporary suppressions, please refer to
// v8_suppressions.js.
// This mocks only architecture specific differences. Refer to v8_mocks.js
// for the general case.
// This file is loaded before each correctness test cases and won't get
// minimized.

// Mock maximum typed-array length and limit to 1MiB.
(function () {
  var mock = function(arrayType) {
    var handler = {
      construct: function(target, args) {
        for (let i = 0; i < args.length; i++) {
          if (typeof args[i] != "object") {
            args[i] = Math.min(1048576, args[i]);
          }
        }
        return new (
            Function.prototype.bind.apply(arrayType, [null].concat(args)));
      },
    };
    return new Proxy(arrayType, handler);
  }

  ArrayBuffer = mock(ArrayBuffer);
  Int8Array = mock(Int8Array);
  Uint8Array = mock(Uint8Array);
  Uint8ClampedArray = mock(Uint8ClampedArray);
  Int16Array = mock(Int16Array);
  Uint16Array = mock(Uint16Array);
  Int32Array = mock(Int32Array);
  Uint32Array = mock(Uint32Array);
  BigInt64Array = mock(BigInt64Array);
  BigUint64Array = mock(BigUint64Array);
  Float32Array = mock(Float32Array);
  Float64Array = mock(Float64Array);
})();

// Mock typed array set function and limit maximum offset to 1MiB.
(function () {
  var typedArrayTypes = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    BigInt64Array,
    BigUint64Array,
    Float32Array,
    Float64Array,
  ];
  for (let typedArrayType of typedArrayTypes) {
    let set = typedArrayType.prototype.set
    typedArrayType.prototype.set = function(array, offset) {
      set.apply(this, [array, offset > 1048576 ? 1048576 : offset])
    };
  }
})();
