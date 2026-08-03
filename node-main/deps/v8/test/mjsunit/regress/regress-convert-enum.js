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

// Flags: --expose-gc

// Create a transition tree A (no descriptors) -> B (descriptor for a) -> C
// (descriptor for a and c), that all share the descriptor array [a,c]. C is the
// owner of the descriptor array.
var o = {};
o.a = 1;
o.c = 2;

// Add a transition B -> D where D has its own descriptor array [a,b] where b is
// a constant function.
var o1 = {};
o1.a = 1;

// Install an enumeration cache in the descriptor array [a,c] at map B.
for (var x in o1) { }
o1.b = function() { return 1; };

// Return ownership of the descriptor array [a,c] to B and trim it to [a].
o = null;
gc();

// Convert the transition B -> D into a transition to B -> E so that E uses the
// instance descriptors [a,b] with b being a field.
var o2 = {};
o2.a = 1;
o2.b = 10;

// Create an object with map B and iterate over it.
var o3 = {};
o3.a = 1;

for (var y in o3) { }
