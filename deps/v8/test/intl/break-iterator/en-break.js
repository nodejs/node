// Copyright 2013 the V8 project authors. All rights reserved.
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

// Segment plain English sentence and check results.

var iterator = new Intl.v8BreakIterator(['en']);

var textToSegment = 'Jack and Jill, went over hill, and got lost. Alert!';
iterator.adoptText(textToSegment);

var slices = [];
var types = [];
var pos = iterator.first();
while (pos !== -1) {
  var nextPos = iterator.next();
  if (nextPos === -1) break;

  slices.push(textToSegment.slice(pos, nextPos));
  types.push(iterator.breakType());

  pos = nextPos;
}

assertEquals('Jack', slices[0]);
assertEquals(' ', slices[1]);
assertEquals('and', slices[2]);
assertEquals(' ', slices[3]);
assertEquals('Jill', slices[4]);
assertEquals(',', slices[5]);
assertEquals('!', slices[slices.length - 1]);

assertEquals('letter', types[0]);
assertEquals('none', types[1]);
assertEquals('letter', types[2]);
assertEquals('none', types[3]);
assertEquals('letter', types[4]);
assertEquals('none', types[types.length - 1]);
