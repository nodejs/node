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

/**
 * @fileoverview Check that flattening deep trees of cons strings does not
 * cause stack overflows.
 */

var depth = 110000;

function newdeep(start) {
  var d = start;
  for (var i = 0; i < depth; i++) {
    d = d + "f";
  }
  return d;
}

var deep = newdeep("foo");
assertEquals('f', deep[0]);

var cmp1 = newdeep("a");
var cmp2 = newdeep("b");

assertEquals(-1, cmp1.localeCompare(cmp2), "ab");

var cmp2empty = newdeep("c");
assertTrue(cmp2empty.localeCompare("") > 0, "c");

var cmp3empty = newdeep("d");
assertTrue("".localeCompare(cmp3empty) < 0), "d";

var slicer = newdeep("slice");

for (i = 0; i < depth + 4; i += 2) {
  slicer =  slicer.slice(1, -1);
}

assertEquals("f", slicer[0]);
assertEquals(1, slicer.length);
