// Copyright 2010 the V8 project authors. All rights reserved.
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

// Test search and replace where we search for a string, not a regexp.

function TestCase(id, expected_output, regexp_source, flags, input) {
  print(id);
  var re = new RegExp(regexp_source, flags);
  var output = input.replace(re, MakeReplaceString);
  assertEquals(expected_output, output, id);
}


function MakeReplaceString() {
  // Arg 0 is the match, n captures follow, n + 1 is offset of match, n + 2 is
  // the subject.
  var l = arguments.length;
  var a = new Array(l - 3);
  a.push(arguments[0]);
  for (var i = 2; i < l - 2; i++) {
    a.push(arguments[i]);
  }
  return "[@" + arguments[l - 2] + ":" + a.join(",") + "]";
}


(function () {
  TestCase(1,
           "ajaxNiceForm.villesHome([@24:#OBJ#])",
           "#OBJ#",
           "g",
           "ajaxNiceForm.villesHome(#OBJ#)");
  TestCase(2,
           "A long string with no non-ASCII characters",
           "Unicode string \u1234",
           "g",
           "A long string with no non-ASCII characters");
})();
