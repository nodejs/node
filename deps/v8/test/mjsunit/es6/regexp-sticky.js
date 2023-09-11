// Copyright 2014 the V8 project authors. All rights reserved.
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

var re = /foo.bar/;

assertTrue(!!"foo*bar".match(re));
assertTrue(!!"..foo*bar".match(re));

var plain = /foobar/;

assertTrue(!!"foobar".match(plain));
assertTrue(!!"..foobar".match(plain));

var sticky = /foo.bar/y;

assertTrue(!!"foo*bar".match(sticky));
assertEquals(7, sticky.lastIndex);
assertFalse(!!"..foo*bar".match(sticky));

var stickyplain = /foobar/y;

assertTrue(!!"foobarfoobar".match(stickyplain));
assertEquals(6, stickyplain.lastIndex);
assertTrue(!!"foobarfoobar".match(stickyplain));
assertEquals(12, stickyplain.lastIndex);
assertFalse(!!"..foobarfoobar".match(stickyplain));

var global = /foo.bar/g;

assertTrue(global.test("foo*bar"));
assertFalse(global.test("..foo*bar"));
global.lastIndex = 0;
assertTrue(global.test("..foo*bar"));

var plainglobal = /foobar/g;

assertTrue(plainglobal.test("foobar"));
assertFalse(plainglobal.test("foobar"));
plainglobal.lastIndex = 0;
assertTrue(plainglobal.test("foobar"));

var stickyglobal = /foo.bar/gy;

assertTrue(stickyglobal.test("foo*bar"));
assertEquals(7, stickyglobal.lastIndex);
assertFalse(stickyglobal.test("..foo*bar"));
stickyglobal.lastIndex = 0;
assertFalse(stickyglobal.test("..foo*bar"));
stickyglobal.lastIndex = 2;
assertTrue(stickyglobal.test("..foo*bar"));
assertEquals(9, stickyglobal.lastIndex);

var stickyplainglobal = /foobar/yg;
assertTrue(stickyplainglobal.sticky);
stickyplainglobal.sticky = false;

assertTrue(stickyplainglobal.test("foobar"));
assertEquals(6, stickyplainglobal.lastIndex);
assertFalse(stickyplainglobal.test("..foobar"));
stickyplainglobal.lastIndex = 0;
assertFalse(stickyplainglobal.test("..foobar"));
stickyplainglobal.lastIndex = 2;
assertTrue(stickyplainglobal.test("..foobar"));
assertEquals(8, stickyplainglobal.lastIndex);

assertEquals("/foo.bar/gy", "" + stickyglobal);
assertEquals("/foo.bar/g", "" + global);

assertTrue(stickyglobal.sticky);
stickyglobal.sticky = false;
assertTrue(stickyglobal.sticky);

var stickyglobal2 = new RegExp("foo.bar", "gy");
assertTrue(stickyglobal2.test("foo*bar"));
assertEquals(7, stickyglobal2.lastIndex);
assertFalse(stickyglobal2.test("..foo*bar"));
stickyglobal2.lastIndex = 0;
assertFalse(stickyglobal2.test("..foo*bar"));
stickyglobal2.lastIndex = 2;
assertTrue(stickyglobal2.test("..foo*bar"));
assertEquals(9, stickyglobal2.lastIndex);

assertEquals("/foo.bar/gy", "" + stickyglobal2);

assertTrue(stickyglobal2.sticky);
stickyglobal2.sticky = false;
assertTrue(stickyglobal2.sticky);

sticky.lastIndex = -1; // Causes sticky regexp to fail fast
assertFalse(sticky.test("..foo.bar"));
assertEquals(0, sticky.lastIndex);

sticky.lastIndex = -1; // Causes sticky regexp to fail fast
assertFalse(!!sticky.exec("..foo.bar"));
assertEquals(0, sticky.lastIndex);

// ES6 draft says: Even when the y flag is used with a pattern, ^ always
// matches only at the beginning of Input, or (if Multiline is true) at the
// beginning of a line.
var hat = /^foo/y;
hat.lastIndex = 2;
assertFalse(hat.test("..foo"));

var mhat = /^foo/my;
mhat.lastIndex = 2;
assertFalse(mhat.test("..foo"));
mhat.lastIndex = 2;
assertTrue(mhat.test(".\nfoo"));

// Check that we don't apply incorrect optimization to sticky regexps that
// are anchored at end.
var stickyanchored = /bar$/y;
assertFalse(stickyanchored.test("foobar"));
stickyanchored.lastIndex = 3;
assertTrue(stickyanchored.test("foobar"));
