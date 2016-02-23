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
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple constructor.
function Point(x,y) {}

// Create mirror for the constructor.
var ctor = debug.MakeMirror(Point);

// Initially no instances.
assertEquals(0, ctor.constructedBy().length);
assertEquals(0, ctor.constructedBy(0).length);
assertEquals(0, ctor.constructedBy(1).length);
assertEquals(0, ctor.constructedBy(10).length);

// Create an instance.
var p = new Point();
assertEquals(1, ctor.constructedBy().length);
assertEquals(1, ctor.constructedBy(0).length);
assertEquals(1, ctor.constructedBy(1).length);
assertEquals(1, ctor.constructedBy(10).length);


// Create 10 more instances making for 11.
ps = [];
for (var i = 0; i < 10; i++) {
  ps.push(new Point());
}
assertEquals(11, ctor.constructedBy().length);
assertEquals(11, ctor.constructedBy(0).length);
assertEquals(1, ctor.constructedBy(1).length);
assertEquals(10, ctor.constructedBy(10).length);
