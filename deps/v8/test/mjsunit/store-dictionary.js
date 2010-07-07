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

// Test dictionary store ICs.

// Function that stores property 'x' on an object.
function store(obj) { obj.x = 42; }

// Create object and force it to dictionary mode by deleting property.
var o = { x: 32, y: 33 };
delete o.y;

// Make the store ic in the 'store' function go into dictionary store
// case.
for (var i = 0; i < 3; i++) {
  store(o);
}
assertEquals(42, o.x);

// Test that READ_ONLY property attribute is respected. Make 'x'
// READ_ONLY.
Object.defineProperty(o, 'x', { value: 32, writable: false });

// Attempt to store using the store ic in the 'store' function.
store(o);

// Check that the store did not change the value.
assertEquals(32, o.x);

// Check that bail-out code works.
// Smi.
store(1);
// Fast case object.
o = new Object();
store(o);
assertEquals(42, o.x);
// Slow case object without x property.
delete o.x;
store(o);
assertEquals(42, o.x);

