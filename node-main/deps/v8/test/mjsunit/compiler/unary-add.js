// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test unary addition in various contexts.

// Test value context.
assertEquals(1, +'1');
assertEquals(1, +1);
assertEquals(1.12, +1.12);
assertEquals(NaN, +undefined);
assertEquals(NaN, +{});

// Test effect context.
assertEquals(1, eval("+'1'; 1"));
assertEquals(1, eval("+1; 1"));
assertEquals(1, eval("+1.12; 1"));
assertEquals(1, eval("+undefined; 1"));
assertEquals(1, eval("+{}; 1"));

// Test test context.
assertEquals(1, (+'1') ? 1 : 2);
assertEquals(1, (+1) ? 1 : 2);
assertEquals(1, (+'0') ? 2 : 1);
assertEquals(1, (+0) ? 2 : 1);
assertEquals(1, (+1.12) ? 1 : 2);
assertEquals(1, (+undefined) ? 2 : 1);
assertEquals(1, (+{}) ? 2 : 1);

// Test value/test context.
assertEquals(1, +'1' || 2);
assertEquals(1, +1 || 2);
assertEquals(1.12, +1.12 || 2);
assertEquals(2, +undefined || 2);
assertEquals(2, +{} || 2);

// Test test/value context.
assertEquals(2, +'1' && 2);
assertEquals(2, +1 && 2);
assertEquals(0, +'0' && 2);
assertEquals(0, +0 && 2);
assertEquals(2, +1.12 && 2);
assertEquals(NaN, +undefined && 2);
assertEquals(NaN, +{} && 2);
