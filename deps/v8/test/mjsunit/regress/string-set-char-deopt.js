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

// Flags: --allow-natives-syntax

(function OneByteSeqStringSetCharDeoptOsr() {
  function deopt() {
    %DeoptimizeFunction(f);
  }

  function f(string, osr) {
    var world = " world";
    %_OneByteSeqStringSetChar(string, 0, (deopt(), 0x48));

    if (osr) while (%GetOptimizationStatus(f) == 2) {}

    return string + world;
  }

  assertEquals("Hello " + "world", f("hello", false));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("Hello " + "world", f("hello", true));
})();


(function OneByteSeqStringSetCharDeopt() {
  function deopt() {
    %DeoptimizeFunction(f);
  }

  function g(x) {
  }

  function f(string) {
    g(%_OneByteSeqStringSetChar(string, 0, (deopt(), 0x48)));
    return string;
  }

  assertEquals("Hell" + "o", f("hello"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("Hell" + "o", f("hello"));
})();


(function TwoByteSeqStringSetCharDeopt() {
  function deopt() {
    %DeoptimizeFunction(f);
  }

  function g(x) {
  }

  function f(string) {
    g(%_TwoByteSeqStringSetChar(string, 0, (deopt(), 0x48)));
    return string;
  }

  assertEquals("Hell" + "o", f("\u20ACello"));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("Hell" + "o", f("\u20ACello"));
})();
