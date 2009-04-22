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

// Ensure that we can correctly change the sign of the most negative smi.

assertEquals(1073741824, -1073741824 * -1);
assertEquals(1073741824, -1073741824 / -1);
assertEquals(1073741824, -(-1073741824));
assertEquals(1073741824, 0 - (-1073741824));

var min_smi = -1073741824;

assertEquals(1073741824, min_smi * -1);
assertEquals(1073741824, min_smi / -1);
assertEquals(1073741824, -min_smi);
assertEquals(1073741824, 0 - min_smi);

var zero = 0;
var minus_one = -1;

assertEquals(1073741824, min_smi * minus_one);
assertEquals(1073741824, min_smi / minus_one);
assertEquals(1073741824, -min_smi);
assertEquals(1073741824, zero - min_smi);

assertEquals(1073741824, -1073741824 * minus_one);
assertEquals(1073741824, -1073741824 / minus_one);
assertEquals(1073741824, -(-1073741824));
assertEquals(1073741824, zero - (-1073741824));

var half_min_smi = -(1<<15);
var half_max_smi = (1<<15);

assertEquals(1073741824, -half_min_smi * half_max_smi);
assertEquals(1073741824, half_min_smi * -half_max_smi);
assertEquals(1073741824, half_max_smi * -half_min_smi);
assertEquals(1073741824, -half_max_smi * half_min_smi);
