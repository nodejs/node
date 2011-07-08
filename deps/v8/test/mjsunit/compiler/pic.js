// Copyright 2010 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

function GetX(o) { return o.x; }
function CallF(o) { return o.f(); }
function SetX(o) { o.x = 42; }
function SetXY(o,y) { return o.x = y; }


function Test(o) {
  SetX(o);
  assertEquals(42, GetX(o));
  assertEquals(87, SetXY(o, 87));
  assertEquals(87, GetX(o));
  assertTrue(SetXY(o, o) === o);
  assertTrue(o === GetX(o), "o === GetX(o)");
  assertEquals("hest", SetXY(o, "hest"));
  assertEquals("hest", GetX(o));
  assertTrue(SetXY(o, Test) === Test);
  assertTrue(Test === GetX(o), "Test === GetX(o)");
  assertEquals(99, CallF(o));
}

// Create a bunch of objects with different layouts.
var o1 = { x: 0, y: 1 };
var o2 = { y: 1, x: 0 };
var o3 = { y: 1, z: 2, x: 0 };
o1.f = o2.f = o3.f = function() { return 99; }

// Run the test until we're fairly sure we've optimized the
// polymorphic property access.
for (var i = 0; i < 5; i++) {
  Test(o1);
  Test(o2);
  Test(o3);
}
%OptimizeFunctionOnNextCall(Test);
Test(o1);
Test(o2);
Test(o3);

// Make sure that the following doesn't crash.
GetX(0);
SetX(0);
SetXY(0, 0);
assertThrows("CallF(0)", TypeError);
