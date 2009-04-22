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

function Construct(x) { return x; }

assertFalse(null == new Construct(null));
assertFalse(void 0 == new Construct(void 0));
assertFalse(0 == new Construct(0));
assertFalse(1 == new Construct(1));
assertFalse(4.2 == new Construct(4.2));
assertFalse('foo' == new Construct('foo'));
assertFalse(true == new Construct(true));

x = {};
assertTrue(x === new Construct(x));
assertFalse(x === new Construct(null));
assertFalse(x === new Construct(void 0));
assertFalse(x === new Construct(1));
assertFalse(x === new Construct(3.2));
assertFalse(x === new Construct(false));
assertFalse(x === new Construct('bar'));
x = [];
assertTrue(x === new Construct(x));
x = new Boolean(true);
assertTrue(x === new Construct(x));
x = new Number(42);
assertTrue(x === new Construct(x));
x = new String('foo');
assertTrue(x === new Construct(x));
x = function() { };
assertTrue(x === new Construct(x));

