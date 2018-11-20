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

(function DeoptimizeArgCallFunctionGeneric() {
  var a = [];

  function f1(method, array, elem, deopt) {
    assertEquals('push', method);
  }

  function f2() { }

  function bar(x, deopt, f) {
    f('push', a, [x], deopt + 0);
  }

  function foo() { return bar(arguments[0], arguments[1], arguments[2]); }
  function baz(f, deopt) { return foo("x", deopt, f); }

  baz(f1, 0);
  baz(f2, 0);
  %OptimizeFunctionOnNextCall(baz);
  baz(f1, "deopt");
})();


(function DeoptimizeArgGlobalFunctionGeneric() {
  var a = [];

  var f1;

  f1 = function(method, array, elem, deopt) {
    assertEquals('push', method);
  }

  function bar(x, deopt, f) {
    f1('push', a, [x], deopt + 0);
  }

  function foo() { return bar(arguments[0], arguments[1]); }
  function baz(deopt) { return foo("x", deopt); }

  baz(0);
  baz(0);
  %OptimizeFunctionOnNextCall(baz);
  baz("deopt");
})();


(function DeoptimizeArgCallFunctionRuntime() {
  var a = [];

  var f1;

  f1 = function(method, array, elem, deopt) {
    assertEquals('push', method);
  }

  function bar(x, deopt) {
    %_CallFunction(null, 'push', [x][0], ((deopt + 0), 1), f1);
  }

  function foo() { return bar(arguments[0], arguments[1]); }
  function baz(deopt) { return foo(0, deopt); }

  baz(0);
  baz(0);
  %OptimizeFunctionOnNextCall(baz);
  baz("deopt");
})();
