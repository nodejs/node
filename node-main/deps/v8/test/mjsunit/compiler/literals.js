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

// Test simple literals.
assertEquals(8, eval("8"));

assertEquals(null, eval("null"));

assertEquals("abc", eval("'abc'"));

assertEquals(8, eval("6;'abc';8"));

// Characters just outside the ranges of hex-escapes.
// "/" comes just before "0".
assertThrows('"\\x1/"');
assertThrows('"\\u111/"');
assertEquals("\\x1\\/", RegExp("\\x1/").source);
assertEquals("\\u111\\/", RegExp("\\u111/").source);

// ":" comes just after "9".
assertThrows('"\\x1:"');
assertThrows('"\\u111:"');
assertEquals("\\x1:", /\x1:/.source);
assertEquals("\\u111:", /\u111:/.source);

// "`" comes just before "a".
assertThrows('"\\x1`"');
assertThrows('"\\u111`"');
assertEquals("\\x1`", /\x1`/.source);
assertEquals("\\u111`", /\u111`/.source);

// "g" comes just before "f".
assertThrows('"\\x1g"');
assertThrows('"\\u111g"');
assertEquals("\\x1g", /\x1g/.source);
assertEquals("\\u111g", /\u111g/.source);

// "@" comes just before "A".
assertThrows('"\\x1@"');
assertThrows('"\\u111@"');
assertEquals("\\x1@", /\x1@/.source);
assertEquals("\\u111@", /\u111@/.source);

// "G" comes just after "F".
assertThrows('"\\x1G"');
assertThrows('"\\u111G"');
assertEquals("\\x1G", /\x1G/.source);
assertEquals("\\u111G", /\u111G/.source);

// Test that octal literals continue to be forbidden in template even
// when followed by a string containing an octal literal.
assertThrows('`\\1`\n"\\1"');

// Test some materialized array literals.
assertEquals([1,2,3,4], eval('[1,2,3,4]'));
assertEquals([[1,2],3,4], eval('[[1,2],3,4]'));
assertEquals([1,[2,3,4]], eval('[1,[2,3,4]]'));

assertEquals([1,2,3,4], eval('var a=1, b=2; [a,b,3,4]'))
assertEquals([1,2,3,4], eval('var a=1, b=2, c = [a,b,3,4]; c'));

function double(x) { return x + x; }
var s = 'var a = 1, b = 2; [double(a), double(b), double(3), double(4)]';
assertEquals([2,4,6,8], eval(s));

// Test array literals in effect context.
assertEquals(17, eval('[1,2,3,4]; 17'));
assertEquals(19, eval('var a=1, b=2; [a,b,3,4]; 19'));
assertEquals(23, eval('var a=1, b=2; c=23; [a,b,3,4]; c'));

// Test that literals work for non-smi indices.
// Ensure hash-map collision if using value as hash.
var o = {"2345678901" : 42, "2345678901" : 30};
