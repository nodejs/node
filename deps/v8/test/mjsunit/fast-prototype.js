// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --noincremental-marking

// Check that objects that are used for prototypes are in the fast mode.

function Super() {
}


function Sub() {
}


function AddProps(obj) {
  for (var i = 0; i < 26; i++) {
    obj["x" + i] = 0;
  }
}
%EnsureFeedbackVectorForFunction(AddProps);


function DoProtoMagic(proto, set__proto__) {
  var receiver;
  if (set__proto__) {
    receiver = new Sub();
    receiver.__proto__ = proto;
  } else {
    Sub.prototype = proto;
    // Need to instantiate Sub to mark .prototype as prototype. Make sure the
    // instantiated object is used so that the allocation is not optimized away.
    receiver = new Sub();
  }
  // Prototypes are made fast when ICs encounter them.
  function ic() { return typeof receiver.foo; }
  %EnsureFeedbackVectorForFunction(ic);
  ic();
  ic();
}
%EnsureFeedbackVectorForFunction(DoProtoMagic);


function test(use_new, add_first, set__proto__) {
  var proto = use_new ? new Super() : {};

  // New object is fast.
  assertTrue(use_new || %HasFastProperties(proto));

  if (add_first) {
    AddProps(proto);
    // Adding this many properties makes it slow.
    assertFalse(%HasFastProperties(proto));
    DoProtoMagic(proto, set__proto__);
    // Making it a prototype makes it fast again.
    assertEquals(!%IsDictPropertyConstTrackingEnabled(),
                 %HasFastProperties(proto));
  } else {
    DoProtoMagic(proto, set__proto__);
    // Still fast
    assertEquals(!%IsDictPropertyConstTrackingEnabled(),
                 %HasFastProperties(proto));
    AddProps(proto);
    assertEquals(!%IsDictPropertyConstTrackingEnabled(),
                 %HasFastProperties(proto));
  }
  return proto;
}
%EnsureFeedbackVectorForFunction(test);

// This test fails easily if gc happens at the wrong time.
gc();

function test_fast_prototype() {
  for (var i = 0; i < 4; i++) {
    var set__proto__ = ((i & 1) != 0);
    var use_new = ((i & 2) != 0);

    test(use_new, true, set__proto__);
    test(use_new, false, set__proto__);
  }


  var x = {a: 1, b: 2, c: 3};
  var o = { __proto__: x };
  assertFalse(%HasFastProperties(x));
  for (key in x) {
    assertTrue(key == 'a');
    break;
  }
  if (!%IsDictPropertyConstTrackingEnabled())
    assertTrue(%HasFastProperties(x));
  delete x.b;
  for (key in x) {
    assertTrue(key == 'a');
    break;
  }

  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(x));
  x.d = 4;
  assertEquals(!%IsDictPropertyConstTrackingEnabled(),
               %HasFastProperties(x));
  for (key in x) {
    assertTrue(key == 'a');
    break;
  }
}
%EnsureFeedbackVectorForFunction(test_fast_prototype);
test_fast_prototype();
