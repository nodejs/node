// Copyright 2011 the V8 project authors. All rights reserved.
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

assertEquals(Boolean(void 0), false);
assertEquals(Boolean(null), false);
assertEquals(Boolean(false), false);
assertEquals(Boolean(true), true);
assertEquals(Boolean(0), false);
assertEquals(Boolean(1), true);
assertEquals(Boolean(assertEquals), true);
assertEquals(Boolean(new Object()), true);
assertTrue(new Boolean(false) !== false);
assertTrue(new Boolean(false) == false);
assertTrue(new Boolean(true) !== true);
assertTrue(new Boolean(true) == true);

assertEquals(true, !false);
assertEquals(false, !true);
assertEquals(true, !!true);
assertEquals(false, !!false);

assertEquals(true, true ? true : false);
assertEquals(false, false ? true : false);

assertEquals(false, true ? false : true);
assertEquals(true, false ? false : true);


assertEquals(true, true && true);
assertEquals(false, true && false);
assertEquals(false, false && true);
assertEquals(false, false && false);

// Regression.
var t = 42;
assertEquals(void 0, t.p);
assertEquals(void 0, t.p && true);
assertEquals(void 0, t.p && false);
assertEquals(void 0, t.p && (t.p == 0));
assertEquals(void 0, t.p && (t.p == null));
assertEquals(void 0, t.p && (t.p == t.p));

var o = new Object();
o.p = 'foo';
assertEquals('foo', o.p);
assertEquals('foo', o.p || true);
assertEquals('foo', o.p || false);
assertEquals('foo', o.p || (o.p == 0));
assertEquals('foo', o.p || (o.p == null));
assertEquals('foo', o.p || (o.p == o.p));
