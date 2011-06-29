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

// Tests from http://blog.stevenlevithan.com/archives/npcg-javascript

assertEquals(true, /(x)?\1y/.test("y"));
assertEquals(["y", undefined], /(x)?\1y/.exec("y"));
assertEquals(["y", undefined], /(x)?y/.exec("y"));
assertEquals(["y", undefined], "y".match(/(x)?\1y/));
assertEquals(["y", undefined], "y".match(/(x)?y/));
assertEquals(["y"], "y".match(/(x)?\1y/g));
assertEquals(["", undefined, ""], "y".split(/(x)?\1y/));
assertEquals(["", undefined, ""], "y".split(/(x)?y/));
assertEquals(0, "y".search(/(x)?\1y/));
assertEquals("z", "y".replace(/(x)?\1y/, "z"));
assertEquals("", "y".replace(/(x)?y/, "$1"));
assertEquals("undefined", "y".replace(/(x)?\1y/,
    function($0, $1){
        return String($1);
    }));
assertEquals("undefined", "y".replace(/(x)?y/,
    function($0, $1){
        return String($1);
    }));
assertEquals("undefined", "y".replace(/(x)?y/,
    function($0, $1){
        return $1;
    }));

// See https://bugzilla.mozilla.org/show_bug.cgi?id=476146
assertEquals(["bbc", "b"], /^(b+|a){1,2}?bc/.exec("bbc"));
assertEquals(["bbaa", "a", "", "a"],
             /((\3|b)\2(a)){2,}/.exec("bbaababbabaaaaabbaaaabba"));

