// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the methods for different TypedArray types have the same
// identity.
// TODO(dehrenberg): Test that the TypedArray proto hierarchy is set
// up properly.
// TODO(dehrenberg): subarray is currently left out because that still
// uses per-type methods. When that's fixed, stop leaving it out.

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

function functionProperties(object) {
  return Object.getOwnPropertyNames(object).filter(function(name) {
    return typeof Object.getOwnPropertyDescriptor(object, name).value
        == "function"
      && name != 'constructor' && name != 'subarray';
  });
}

var typedArrayMethods = functionProperties(Uint8Array.prototype);
var typedArrayClassMethods = functionProperties(Uint8Array);

for (var constructor of typedArrayConstructors) {
  for (var method of typedArrayMethods) {
    assertEquals(constructor.prototype[method],
                 Uint8Array.prototype[method], method);
  }
  for (var classMethod of typedArrayClassMethods) {
    assertEquals(constructor[method], Uint8Array[method], classMethod);
  }
}
