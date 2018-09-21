// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];

function TestTypedArrayMap(constructor) {
  assertEquals(1, constructor.prototype.map.length);

  var target;

  class EscapingArray extends constructor {
    constructor(...args) {
      super(...args);
      target = this;
    }
  }

  class DetachingArray extends constructor {
    static get [Symbol.species]() {
      return EscapingArray;
    }
  }

  assertThrows(function(){
    new DetachingArray(5).map(function(v,i,a){
      print(i);
      if (i == 1) {
        %ArrayBufferNeuter(target.buffer);
      }
    })
  }, TypeError);

}

for (i = 0; i < typedArrayConstructors.length; i++) {
  TestTypedArrayMap(typedArrayConstructors[i]);
}
