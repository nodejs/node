// Copyright 2012 the V8 project authors. All rights reserved.
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

// Make sure the strings are long enough to trigger the one-char string replace.
var prefix1024 = "0123456789ABCDEF";
for (var i = 0; i < 6; i++) prefix1024 += prefix1024;

function test_replace(result, expected, search, replace) {
  assertEquals(expected, result.replace(search, replace));
}

// '$' in the replace string.
test_replace(prefix1024 + "abcdefghijklmnopqrstuvwxyz",
             prefix1024 + "abcdefghijk#l#mnopqrstuvwxyz",
             "l", "#$&#");

test_replace(prefix1024 + "abcdefghijklmnopqrstuvwxyz\u1234",
             prefix1024 + "abcdefghijk\u2012l\u2012mnopqrstuvwxyz\u1234",
             "l", "\u2012$&\u2012");

test_replace(prefix1024 + "abcdefghijklmnopqrstuvwxyz",
             prefix1024 + "abcdefghijk$mnopqrstuvwxyz",
             "l", "$$");

test_replace(prefix1024 + "abcdefghijklmnopqrstuvwxyz\u1234",
             prefix1024 + "abcdefghijk$mnopqrstuvwxyz\u1234",
             "l", "$$");

// Zero length replace string.
test_replace(prefix1024 + "abcdefghijklmnopqrstuvwxyz",
             prefix1024 + "abcdefghijklmnopqstuvwxyz",
             "r", "");

test_replace(prefix1024 + "abcdefghijklmnopq\u1234stuvwxyz",
             prefix1024 + "abcdefghijklmnopqstuvwxyz",
             "\u1234", "");

// Search char not found.
var not_found_1 = prefix1024 + "abcdefghijklmnopqrstuvwxyz";
test_replace(not_found_1, not_found_1, "+", "-");

var not_found_2 = prefix1024 + "abcdefghijklm\u1234nopqrstuvwxyz";
test_replace(not_found_2, not_found_2, "+", "---");

var not_found_3 = prefix1024 + "abcdefghijklmnopqrstuvwxyz";
test_replace(not_found_3, not_found_3, "\u1234", "ZZZ");

// Deep cons tree.
var nested_1 = "";
for (var i = 0; i < 100000; i++) nested_1 += "y";
var nested_1_result = prefix1024 + nested_1 + "aa";
nested_1 = prefix1024 + nested_1 + "z";
test_replace(nested_1, nested_1_result, "z", "aa");

var nested_2 = "\u2244";
for (var i = 0; i < 100000; i++) nested_2 += "y";
var nested_2_result = prefix1024 + nested_2 + "aa";
nested_2 = prefix1024 + nested_2 + "\u2012";
test_replace(nested_2, nested_2_result, "\u2012", "aa");

// Sliced string as input.  A cons string is always flattened before sliced.
var slice_1 = ("ab" + prefix1024 + "cdefghijklmnopqrstuvwxyz").slice(1, -1);
var slice_1_result = "b" + prefix1024 + "cdefghijklmnopqrstuvwxQ";
test_replace(slice_1, slice_1_result, "y", "Q");

var slice_2 = (prefix1024 + "abcdefghijklmno\u1234\u1234p").slice(1, -1);
var slice_2_result = prefix1024.substr(1) + "abcdefghijklmnoQ\u1234";
test_replace(slice_2, slice_2_result, "\u1234", "Q");
