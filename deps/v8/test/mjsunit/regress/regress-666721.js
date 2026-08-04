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

function len0(a) { return a.length; }
function len1(a) { return a.length; }
function len2(a) { return a.length; }
function len3(a) { return a.length; }

assertEquals(0, len0([]));
assertEquals(1, len0({length:1}));
assertEquals(2, len0([1,2]));
assertEquals(3, len0('123'));

assertEquals(0, len1(''));
assertEquals(1, len1({length:1}));
assertEquals(2, len1('12'));
assertEquals(3, len1([1,2,3]));

assertEquals(0, len2({length:0}));
assertEquals(1, len2([1]));
assertEquals(2, len2({length:2}));
assertEquals(3, len2([1,2,3]));
assertEquals(4, len2('1234'));

assertEquals(0, len3({length:0}));
assertEquals(1, len3('1'));
assertEquals(2, len3({length:2}));
assertEquals(3, len3('123'));
assertEquals(4, len3([1,2,3,4]));
