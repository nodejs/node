// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, code_stubs) {

"use strict";

code_stubs.StringLengthTFStub = function StringLengthTFStub(call_conv, minor_key) {
  var stub = function(receiver, name, i, v) {
    // i and v are dummy parameters mandated by the InterfaceDescriptor,
    // (LoadWithVectorDescriptor).
    return %_StringGetLength(%_JSValueGetValue(receiver));
  }
  return stub;
}

code_stubs.StringAddTFStub = function StringAddTFStub(call_conv, minor_key) {
  var stub = function(left, right) {
    return %StringAdd(left, right);
  }
  return stub;
}

const kTurboFanICCallModeMask = 1;
const kTurboFanICCallForUnptimizedCode = 0;
const kTurboFanICCallForOptimizedCode = 1;

code_stubs.MathFloorStub = function MathFloorStub(call_conv, minor_key) {
  var call_from_optimized_ic = function(f, i, tv, receiver, v) {
    "use strict";
    // |f| is this function's JSFunction
    // |i| is TypeFeedbackVector slot # of callee's CallIC for Math.floor call
    // |receiver| is receiver, should not be used
    // |tv| is the calling function's type vector
    // |v| is the value to floor
    if (f !== %_FixedArrayGet(tv, i|0)) {
      return %_CallFunction(receiver, v, f);
    }
    var r = %_MathFloor(+v);
    if (%_IsMinusZero(r)) {
      // Collect type feedback when the result of the floor is -0. This is
      // accomplished by storing a sentinel in the second, "extra"
      // TypeFeedbackVector slot corresponding to the Math.floor CallIC call in
      // the caller's TypeVector.
      %_FixedArraySet(tv, ((i|0)+1)|0, 1);
      return -0;
    }
    // Return integers in smi range as smis.
    var trunc = r|0;
    if (trunc === r) {
      return trunc;
    }
    return r;
  }
  var call_mode = (minor_key & kTurboFanICCallModeMask);
  if (call_mode == kTurboFanICCallForOptimizedCode) {
    return call_from_optimized_ic;
  } else {
    %SetForceInlineFlag(call_from_optimized_ic);
    var call_from_unoptimized_ic = function(f, i, receiver, v) {
      var tv = %_GetTypeFeedbackVector(%_GetCallerJSFunction());
      return call_from_optimized_ic(f, i, tv, receiver, v);
    }
    return call_from_unoptimized_ic;
  }
}

})
