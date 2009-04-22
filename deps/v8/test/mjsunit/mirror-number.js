// Copyright 2008 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug
// Test the mirror object for number values

function testNumberMirror(n) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(n);
  var serializer = debug.MakeMirrorSerializer();
  var json = serializer.serializeValue(mirror);

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.NumberMirror);

  // Check the mirror properties.
  assertTrue(mirror.isNumber());
  assertEquals('number', mirror.type());
  assertTrue(mirror.isPrimitive());

  // Test text representation
  assertEquals(String(n), mirror.toText());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('number', fromJSON.type);
  if (!isNaN(n)) {
    assertEquals(n, fromJSON.value);
  } else {
    // NaN values are encoded as strings.
    assertTrue(typeof fromJSON.value == 'string');
    if (n === Infinity) {
      assertEquals('Infinity', fromJSON.value);
    } else if (n === -Infinity) {
      assertEquals('-Infinity', fromJSON.value);
    } else {
      assertEquals('NaN', fromJSON.value);
    }
  }
}


// Test a number of different numbers.
testNumberMirror(-7);
testNumberMirror(-6.5);
testNumberMirror(0);
testNumberMirror(42);
testNumberMirror(100.0002);
testNumberMirror(Infinity);
testNumberMirror(-Infinity);
testNumberMirror(NaN);
