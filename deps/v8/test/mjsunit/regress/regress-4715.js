// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

var training = {};
training.a = "nop";
training.slow = "nop";
delete training.slow;  // Dictionary-mode properties => slow-mode for-in.

var keepalive = {};
keepalive.a = "nop";  // Keep a map early in the transition chain alive.

function GetReal() {
  var r = {};
  r.a = "nop";
  r.b = "nop";
  r.c = "dictionarize",
  r.d = "gc";
  r.e = "result";
  return r;
};

function SideEffect(object, action) {
  if (action === "dictionarize") {
    delete object.a;
  } else if (action === "gc") {
    gc();
  }
}

function foo(object) {
  for (var key in object) {
    SideEffect(object, object[key]);
  }
  return key;
}

// Collect type feedback for slow-mode for-in.
foo(training);
SideEffect({a: 0}, "dictionarize");
SideEffect({}, "gc");

// Compile for slow-mode objects...
%OptimizeFunctionOnNextCall(foo);
// ...and pass in a fast-mode object.
assertEquals("e", foo(GetReal()));
