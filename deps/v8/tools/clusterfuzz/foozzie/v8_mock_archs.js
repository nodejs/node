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

// Mock maximum typed-array buffer and limit to 1MiB. Otherwise we might
// get range errors. We ignore those by crashing, but that reduces coverage,
// hence, let's reduce the range-error rate.
(function() {
  // Math.min might be manipulated in test cases.
  const min = Math.min;
  const maxBytes = 1048576;
  const mock = function(type) {
    const maxLength = maxBytes / (type.BYTES_PER_ELEMENT || 1);
    const handler = {
      construct: function(target, args) {
        if (args[0] && typeof args[0] != "object") {
          // Length used as first argument.
          args[0] = min(maxLength, Number(args[0]));
        } else if (args[0] instanceof ArrayBuffer && args.length > 1) {
          // Buffer used as first argument.
          const buffer = args[0];
          args[1] = Number(args[1]);
          // Ensure offset is multiple of bytes per element.
          args[1] = args[1] - (args[1] % type.BYTES_PER_ELEMENT);
          // Limit offset to length of buffer.
          args[1] = min(args[1], buffer.byteLength || 0);
          if (args.length > 2) {
            // If also length is given, limit it to the maximum that's possible
            // given buffer and offset.
            const maxBytesLeft = buffer.byteLength - args[1];
            const maxLengthLeft = maxBytesLeft / type.BYTES_PER_ELEMENT;
            args[2] = min(Number(args[2]), maxLengthLeft);
          }
        }
        return new (Function.prototype.bind.apply(type, [null].concat(args)));
      },
    };
    return new Proxy(type, handler);
  }

  ArrayBuffer = mock(ArrayBuffer);
  SharedArrayBuffer = mock(SharedArrayBuffer);
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

// Mock typed array set function and cap offset to not throw a range error.
(function() {
  // Math.min might be manipulated in test cases.
  const min = Math.min;
  const types = [
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
  for (const type of types) {
    const set = type.prototype.set;
    type.prototype.set = function(array, offset) {
      if (Array.isArray(array)) {
        offset = Number(offset);
        offset = min(offset, this.length - array.length);
      }
      set.call(this, array, offset);
    };
  }
})();
