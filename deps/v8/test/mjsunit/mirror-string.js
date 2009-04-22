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
// Test the mirror object for string values

const kMaxProtocolStringLength = 80; // Constant from mirror-delay.js

function testStringMirror(s) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(s);
  var serializer = debug.MakeMirrorSerializer();
  var json = serializer.serializeValue(mirror);

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.StringMirror);

  // Check the mirror properties.
  assertTrue(mirror.isString());
  assertEquals('string', mirror.type());
  assertTrue(mirror.isPrimitive());

  // Test text representation
  if (s.length <= kMaxProtocolStringLength) {
    assertEquals(s, mirror.toText());
  } else {
    assertEquals(s.substring(0, kMaxProtocolStringLength),
                 mirror.toText().substring(0, kMaxProtocolStringLength));
  }

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('string', fromJSON.type);
  if (s.length <= kMaxProtocolStringLength) {
    assertEquals(s, fromJSON.value);
  } else {
    assertEquals(s.substring(0, kMaxProtocolStringLength),
                 fromJSON.value.substring(0, kMaxProtocolStringLength));
    assertEquals(fromJSON.fromIndex, 0);
    assertEquals(fromJSON.toIndex, kMaxProtocolStringLength);
  }
}

// Test a number of different strings.
testStringMirror('');
testStringMirror('abcdABCD');
testStringMirror('1234');
testStringMirror('"');
testStringMirror('"""');
testStringMirror("'");
testStringMirror("'''");
testStringMirror("'\"'");
testStringMirror('\\');
testStringMirror('\b\t\n\f\r');
testStringMirror('\u0001\u0002\u001E\u001F');
testStringMirror('"a":1,"b":2');

var s = "1234567890"
s = s + s + s + s + s + s + s + s;
assertEquals(kMaxProtocolStringLength, s.length);
testStringMirror(s);
s = s + 'X';
testStringMirror(s);
