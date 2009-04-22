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

// Ensure that operations on small integers handle -0.

var zero = 0;
var one = 1;
var minus_one = -1;
var two = 2;
var four = 4;
var minus_two = -2;
var minus_four = -4;

// variable op variable

assertEquals(one / (-zero), -Infinity);

assertEquals(one / (zero * minus_one), -Infinity);
assertEquals(one / (minus_one * zero), -Infinity);
assertEquals(one / (zero * zero), Infinity);
assertEquals(one / (minus_one * minus_one), 1);

assertEquals(one / (zero / minus_one), -Infinity);
assertEquals(one / (zero / one), Infinity);

assertEquals(one / (minus_four % two), -Infinity);
assertEquals(one / (minus_four % minus_two), -Infinity);
assertEquals(one / (four % two), Infinity);
assertEquals(one / (four % minus_two), Infinity);

// literal op variable

assertEquals(one / (0 * minus_one), -Infinity);
assertEquals(one / (-1 * zero), -Infinity);
assertEquals(one / (0 * zero), Infinity);
assertEquals(one / (-1 * minus_one), 1);

assertEquals(one / (0 / minus_one), -Infinity);
assertEquals(one / (0 / one), Infinity);

assertEquals(one / (-4 % two), -Infinity);
assertEquals(one / (-4 % minus_two), -Infinity);
assertEquals(one / (4 % two), Infinity);
assertEquals(one / (4 % minus_two), Infinity);

// variable op literal

assertEquals(one / (zero * -1), -Infinity);
assertEquals(one / (minus_one * 0), -Infinity);
assertEquals(one / (zero * 0), Infinity);
assertEquals(one / (minus_one * -1), 1);

assertEquals(one / (zero / -1), -Infinity);
assertEquals(one / (zero / 1), Infinity);

assertEquals(one / (minus_four % 2), -Infinity);
assertEquals(one / (minus_four % -2), -Infinity);
assertEquals(one / (four % 2), Infinity);
assertEquals(one / (four % -2), Infinity);

// literal op literal

assertEquals(one / (-0), -Infinity);

assertEquals(one / (0 * -1), -Infinity);
assertEquals(one / (-1 * 0), -Infinity);
assertEquals(one / (0 * 0), Infinity);
assertEquals(one / (-1 * -1), 1);

assertEquals(one / (0 / -1), -Infinity);
assertEquals(one / (0 / 1), Infinity);

assertEquals(one / (-4 % 2), -Infinity);
assertEquals(one / (-4 % -2), -Infinity);
assertEquals(one / (4 % 2), Infinity);
assertEquals(one / (4 % -2), Infinity);
