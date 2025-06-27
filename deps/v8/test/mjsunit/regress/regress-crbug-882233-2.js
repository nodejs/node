// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Intended to test bug [882233] on TF inlined (js-call-reducer) path.

function shift_array() {
  let array = [];
  Object.defineProperty(array, 'length', {writable : false});
  out = array; // Prevent array's map from dying too early.
  return array.shift();
}

%PrepareFunctionForOptimization(shift_array);
assertThrows(shift_array);
assertThrows(shift_array);
%OptimizeFunctionOnNextCall(shift_array);
assertThrows(shift_array);
assertOptimized(shift_array);


function shift_object() {
  let object = { length: 0 };
  Object.defineProperty(object, 'length', {writable : false});
  out = object; // Prevent object's map from dying too early.
  return object.shift();
}

%PrepareFunctionForOptimization(shift_object);
assertThrows(shift_object);
assertThrows(shift_object);
%OptimizeFunctionOnNextCall(shift_object);
assertThrows(shift_object);
assertOptimized(shift_object);
