// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function TestBuiltinSubclassing(Builtin) {
  assertTrue(%HasFastProperties(Builtin));
  assertTrue(%HasFastProperties(Builtin.prototype));
  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(Builtin.prototype.__proto__));

  class SubClass extends Builtin {}

  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(Builtin));
  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(Builtin.prototype));
  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(Builtin.prototype.__proto__));

}

let TypedArray = Uint8Array.__proto__;

TestBuiltinSubclassing(RegExp);
TestBuiltinSubclassing(Promise);
TestBuiltinSubclassing(Array);
TestBuiltinSubclassing(TypedArray);
TestBuiltinSubclassing(Uint8Array);
TestBuiltinSubclassing(Int8Array);
TestBuiltinSubclassing(Uint16Array);
TestBuiltinSubclassing(Int16Array);
TestBuiltinSubclassing(Uint32Array);
TestBuiltinSubclassing(Int32Array);
TestBuiltinSubclassing(Float32Array);
TestBuiltinSubclassing(Float64Array);
TestBuiltinSubclassing(Uint8ClampedArray);
