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
// Test the mirror object for boolean values

function testDateMirror(d, iso8601) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(d);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ObjectMirror);
  assertTrue(mirror instanceof debug.DateMirror);

  // Check the mirror properties.
  assertTrue(mirror.isDate());
  assertEquals('object', mirror.type());
  assertFalse(mirror.isPrimitive());

  // Test text representation
  assertEquals(iso8601, mirror.toText());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('object', fromJSON.type);
  assertEquals('Date', fromJSON.className);
  assertEquals(iso8601, fromJSON.value);
}

// Test Date values.
testDateMirror(new Date(Date.parse("Dec 25, 1995 1:30 UTC")),
               "1995-12-25T01:30:00.000Z");
d = new Date();
d.setUTCFullYear(1967);
d.setUTCMonth(0); // January.
d.setUTCDate(17);
d.setUTCHours(9);
d.setUTCMinutes(22);
d.setUTCSeconds(59);
d.setUTCMilliseconds(0);
testDateMirror(d, "1967-01-17T09:22:59.000Z");
d.setUTCMilliseconds(1);
testDateMirror(d, "1967-01-17T09:22:59.001Z");
d.setUTCSeconds(12);
testDateMirror(d, "1967-01-17T09:22:12.001Z");
d.setUTCSeconds(36);
testDateMirror(d, "1967-01-17T09:22:36.001Z");
d.setUTCMilliseconds(136);
testDateMirror(d, "1967-01-17T09:22:36.136Z");
