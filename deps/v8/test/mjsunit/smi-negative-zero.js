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

assertEquals(one / (-zero), -Infinity, "one / -0 I");

assertEquals(one / (zero * minus_one), -Infinity, "one / -1");
assertEquals(one / (minus_one * zero), -Infinity, "one / -0 II");
assertEquals(one / (zero * zero), Infinity, "one / 0 I");
assertEquals(one / (minus_one * minus_one), 1, "one / 1");

assertEquals(one / (zero / minus_one), -Infinity, "one / -0 III");
assertEquals(one / (zero / one), Infinity, "one / 0 II");

assertEquals(one / (minus_four % two), -Infinity, "foo1");
assertEquals(one / (minus_four % minus_two), -Infinity, "foo2");
assertEquals(one / (four % two), Infinity, "foo3");
assertEquals(one / (four % minus_two), Infinity, "foo4");

// literal op variable

assertEquals(one / (0 * minus_one), -Infinity, "bar1");
assertEquals(one / (-1 * zero), -Infinity, "bar2");
assertEquals(one / (0 * zero), Infinity, "bar3");
assertEquals(one / (-1 * minus_one), 1, "bar4");

assertEquals(one / (0 / minus_one), -Infinity, "baz1");
assertEquals(one / (0 / one), Infinity, "baz2");

assertEquals(one / (-4 % two), -Infinity, "baz3");
assertEquals(one / (-4 % minus_two), -Infinity, "baz4");
assertEquals(one / (4 % two), Infinity, "baz5");
assertEquals(one / (4 % minus_two), Infinity, "baz6");

// variable op literal

assertEquals(one / (zero * -1), -Infinity, "fizz1");
assertEquals(one / (minus_one * 0), -Infinity, "fizz2");
assertEquals(one / (zero * 0), Infinity, "fizz3");
assertEquals(one / (minus_one * -1), 1, "fizz4");

assertEquals(one / (zero / -1), -Infinity, "buzz1");
assertEquals(one / (zero / 1), Infinity, "buzz2");

assertEquals(one / (minus_four % 2), -Infinity, "buzz3");
assertEquals(one / (minus_four % -2), -Infinity, "buzz4");
assertEquals(one / (four % 2), Infinity, "buzz5");
assertEquals(one / (four % -2), Infinity, "buzz6");

// literal op literal

assertEquals(one / (-0), -Infinity, "fisk1");

assertEquals(one / (0 * -1), -Infinity, "fisk2");
assertEquals(one / (-1 * 0), -Infinity, "fisk3");
assertEquals(one / (0 * 0), Infinity, "fisk4");
assertEquals(one / (-1 * -1), 1, "fisk5");

assertEquals(one / (0 / -1), -Infinity, "hest1");
assertEquals(one / (0 / 1), Infinity, "hest2");

assertEquals(one / (-4 % 2), -Infinity, "fiskhest1");
assertEquals(one / (-4 % -2), -Infinity, "fiskhest2");
assertEquals(one / (4 % 2), Infinity, "fiskhest3");
assertEquals(one / (4 % -2), Infinity, "fiskhest4");
