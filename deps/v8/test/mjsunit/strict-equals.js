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

var n = null;
var u = void 0;
assertTrue(null === null);
assertTrue(null === n);
assertTrue(n === null);
assertTrue(n === n);
assertFalse(null === void 0);
assertFalse(void 0 === null);
assertFalse(u === null);
assertFalse(null === u);
assertFalse(n === u);
assertFalse(u === n);
assertTrue(void 0 === void 0);
assertTrue(u === u);
assertTrue(u === void 0);
assertTrue(void 0 === u);

assertTrue('foo' === 'foo');
assertFalse('bar' === 'foo');
assertFalse('foo' === new String('foo'));
assertFalse(new String('foo') === new String('foo'));
var s = new String('foo');
assertTrue(s === s);
assertFalse(s === null);
assertFalse(s === void 0);
assertFalse('foo' === null);
assertFalse('foo' === 7);
assertFalse('foo' === true);
assertFalse('foo' === void 0);
assertFalse('foo' === {});

assertFalse({} === {});
var x = {};
assertTrue(x === x);
assertFalse(x === null);
assertFalse(x === 7);
assertFalse(x === true);
assertFalse(x === void 0);
assertFalse(x === {});

assertTrue(true === true);
assertTrue(false === false);
assertFalse(false === true);
assertFalse(true === false);
assertFalse(true === new Boolean(true));
assertFalse(true === new Boolean(false));
assertFalse(false === new Boolean(true));
assertFalse(false === new Boolean(false));
assertFalse(true === 0);
assertFalse(true === 1);

assertTrue(0 === 0);
assertTrue(-0 === -0);
assertTrue(-0 === 0);
assertTrue(0 === -0);
assertFalse(0 === new Number(0));
assertFalse(1 === new Number(1));
assertTrue(4.2 === 4.2);
assertTrue(4.2 === Number(4.2));




