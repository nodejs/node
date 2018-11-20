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


function TryGetPrototypeOfNonObject(x) {
  var caught = 0;
  try {
      Object.getPrototypeOf(x);
  } catch (e) {
    caught = e;
  }

  assertTrue(caught instanceof TypeError);
};

function GetPrototypeOfObject(x) {
  assertDoesNotThrow(Object.getPrototypeOf(x));
  assertNotNull(Object.getPrototypeOf(x));
  assertEquals(Object.getPrototypeOf(x), x.__proto__);
}

function F(){};

// Non object
var x = 10;

// Object
var y = new F();

// Make sure that TypeError exceptions are thrown when non-objects are passed
// as argument
TryGetPrototypeOfNonObject(0);
TryGetPrototypeOfNonObject(null);
TryGetPrototypeOfNonObject('Testing');
TryGetPrototypeOfNonObject(x);

// Make sure the real objects have this method and that it returns the
// actual prototype object. Also test for Functions and RegExp.
GetPrototypeOfObject(this);
GetPrototypeOfObject(y);
GetPrototypeOfObject({x:5});
GetPrototypeOfObject(F);
GetPrototypeOfObject(RegExp);
