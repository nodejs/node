// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function P() {
  this.a0 = {};
  this.a1 = {};
  this.a2 = {};
  this.a3 = {};
  this.a4 = {};
}

function A() {
}

var proto = new P();
A.prototype = proto;

function foo(o) {
  return o.a0;
}
%EnsureFeedbackVectorForFunction(foo);

// Ensure |proto| is in old space.
gc();
gc();
gc();

// Ensure |proto| is marked as "should be fast".
var o = new A();
foo(o);
foo(o);
foo(o);
assertTrue(%HasFastProperties(proto));

// Contruct a double value that looks like a tagged pointer.
var buffer = new ArrayBuffer(8);
var int32view = new Int32Array(buffer);
var float64view = new Float64Array(buffer);
int32view[0] = int32view[1] = 0x40000001;
var boom = float64view[0];


// Write new space object.
proto.a4 = {a: 0};
// Immediately delete the field.
delete proto.a4;

// |proto| must sill be fast.
assertTrue(%HasFastProperties(proto));

// Add a double field instead of deleted a4 that looks like a tagged pointer.
proto.boom = boom;

// Boom!
gc();
