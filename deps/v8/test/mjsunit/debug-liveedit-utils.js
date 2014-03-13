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

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.

Debug = debug.Debug

function Return2010() {
  return 2010;
}


// Diff it trivial: zero chunks
var NoChunkTranslator = new Debug.LiveEdit.TestApi.PosTranslator([]);

assertEquals(0, NoChunkTranslator.Translate(0));
assertEquals(10, NoChunkTranslator.Translate(10));


// Diff has one chunk
var SingleChunkTranslator = new Debug.LiveEdit.TestApi.PosTranslator([20, 30, 25]);

assertEquals(0, SingleChunkTranslator.Translate(0));
assertEquals(5, SingleChunkTranslator.Translate(5));
assertEquals(10, SingleChunkTranslator.Translate(10));
assertEquals(19, SingleChunkTranslator.Translate(19));
assertEquals(2010, SingleChunkTranslator.Translate(20, Return2010));
assertEquals(25, SingleChunkTranslator.Translate(30));
assertEquals(26, SingleChunkTranslator.Translate(31));
assertEquals(2010, SingleChunkTranslator.Translate(26, Return2010));

try {
  SingleChunkTranslator.Translate(21);
  assertTrue(false);
} catch (ignore) {
}
try {
  SingleChunkTranslator.Translate(24);
  assertTrue(false);
} catch (ignore) {
}


// Diff has several chunk (3). See the table below.

/*
chunks: (new <- old)
 10   10
 15   20

 35   40
 50   40

 70   60
 70   70
*/

var MultiChunkTranslator = new Debug.LiveEdit.TestApi.PosTranslator([10, 20, 15, 40, 40, 50, 60, 70, 70 ]);
assertEquals(5, MultiChunkTranslator.Translate(5));
assertEquals(9, MultiChunkTranslator.Translate(9));
assertEquals(2010, MultiChunkTranslator.Translate(10, Return2010));
assertEquals(15, MultiChunkTranslator.Translate(20));
assertEquals(20, MultiChunkTranslator.Translate(25));
assertEquals(34, MultiChunkTranslator.Translate(39));
assertEquals(50, MultiChunkTranslator.Translate(40, Return2010));
assertEquals(55, MultiChunkTranslator.Translate(45));
assertEquals(69, MultiChunkTranslator.Translate(59));
assertEquals(2010, MultiChunkTranslator.Translate(60, Return2010));
assertEquals(70, MultiChunkTranslator.Translate(70));
assertEquals(75, MultiChunkTranslator.Translate(75));
