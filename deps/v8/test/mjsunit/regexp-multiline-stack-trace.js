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

// The flags below are to test the trace-calls functionality and the
// preallocated meessage memory.
// Flags: --trace-calls --preallocate-message-memory

/**
 * @fileoverview Check that various regexp constructs work as intended.
 * Particularly those regexps that use ^ and $.
 */

assertTrue(/^bar/.test("bar"));
assertTrue(/^bar/.test("bar\nfoo"));
assertFalse(/^bar/.test("foo\nbar"));
assertTrue(/^bar/m.test("bar"));
assertTrue(/^bar/m.test("bar\nfoo"));
assertTrue(/^bar/m.test("foo\nbar"));

assertTrue(/bar$/.test("bar"));
assertFalse(/bar$/.test("bar\nfoo"));
assertTrue(/bar$/.test("foo\nbar"));
assertTrue(/bar$/m.test("bar"));
assertTrue(/bar$/m.test("bar\nfoo"));
assertTrue(/bar$/m.test("foo\nbar"));

assertFalse(/^bxr/.test("bar"));
assertFalse(/^bxr/.test("bar\nfoo"));
assertFalse(/^bxr/m.test("bar"));
assertFalse(/^bxr/m.test("bar\nfoo"));
assertFalse(/^bxr/m.test("foo\nbar"));

assertFalse(/bxr$/.test("bar"));
assertFalse(/bxr$/.test("foo\nbar"));
assertFalse(/bxr$/m.test("bar"));
assertFalse(/bxr$/m.test("bar\nfoo"));
assertFalse(/bxr$/m.test("foo\nbar"));


assertTrue(/^.*$/.test(""));
assertTrue(/^.*$/.test("foo"));
assertFalse(/^.*$/.test("\n"));
assertTrue(/^.*$/m.test("\n"));

assertTrue(/^[\s]*$/.test(" "));
assertTrue(/^[\s]*$/.test("\n"));

assertTrue(/^[^]*$/.test(""));
assertTrue(/^[^]*$/.test("foo"));
assertTrue(/^[^]*$/.test("\n"));

assertTrue(/^([()\s]|.)*$/.test("()\n()"));
assertTrue(/^([()\n]|.)*$/.test("()\n()"));
assertFalse(/^([()]|.)*$/.test("()\n()"));
assertTrue(/^([()]|.)*$/m.test("()\n()"));
assertTrue(/^([()]|.)*$/m.test("()\n"));
assertTrue(/^[()]*$/m.test("()\n."));

assertTrue(/^[\].]*$/.test("...]..."));


function check_case(lc, uc) {
  var a = new RegExp("^" + lc + "$");
  assertFalse(a.test(uc));
  a = new RegExp("^" + lc + "$", "i");
  assertTrue(a.test(uc));

  var A = new RegExp("^" + uc + "$");
  assertFalse(A.test(lc));
  A = new RegExp("^" + uc + "$", "i");
  assertTrue(A.test(lc));

  a = new RegExp("^[" + lc + "]$");
  assertFalse(a.test(uc));
  a = new RegExp("^[" + lc + "]$", "i");
  assertTrue(a.test(uc));

  A = new RegExp("^[" + uc + "]$");
  assertFalse(A.test(lc));
  A = new RegExp("^[" + uc + "]$", "i");
  assertTrue(A.test(lc));
}


check_case("a", "A");
// Aring
check_case(String.fromCharCode(229), String.fromCharCode(197));
// Russian G
check_case(String.fromCharCode(0x413), String.fromCharCode(0x433));


assertThrows("a = new RegExp('[z-a]');");
