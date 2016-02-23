// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test that functions introduced by eval work both when there are
// strict mode and non-strict mode eval in scopes.

// Flags: --allow-natives-syntax

var x = 27;

function f() { return x; }

assertEquals(27, f());

function do_eval(str) {
  "use strict";
  return eval(str);
}

var eval_f = do_eval('(' + f + ')');
for (var i = 0; i < 5; i++) assertEquals(27, eval_f());
%OptimizeFunctionOnNextCall(eval_f);
assertEquals(27, eval_f());

function do_eval_local(str) {
  "use strict";
  var x = 42;
  return eval(str);
}

eval_f = do_eval_local('(' + f + ')');
for (var i = 0; i < 5; i++) assertEquals(42, eval_f());
%OptimizeFunctionOnNextCall(eval_f);
assertEquals(42, eval_f());

function do_eval_with_other_eval_call(str) {
  "use strict";
  var f = eval(str);
  eval('var x = 1');
  return f;
}

eval_f = do_eval_with_other_eval_call('(' + f + ')');
for (var i = 0; i < 5; i++) assertEquals(27, eval_f());
%OptimizeFunctionOnNextCall(eval_f);
assertEquals(27, eval_f());

function test_non_strict_outer_eval() {
  function strict_eval(str) { "use strict"; return eval(str); }
  var eval_f = strict_eval('(' + f + ')');
  for (var i = 0; i < 5; i++) assertEquals(27, eval_f());
  %OptimizeFunctionOnNextCall(eval_f);
  assertEquals(27, eval_f());
  eval("var x = 3");
  assertEquals(3, eval_f());
}

test_non_strict_outer_eval();

function test_strict_outer_eval() {
  "use strict";
  function strict_eval(str) { "use strict"; return eval(str); }
  var eval_f = strict_eval('(' + f + ')');
  for (var i = 0; i < 5; i++) assertEquals(27, eval_f());
  %OptimizeFunctionOnNextCall(eval_f);
  assertEquals(27, eval_f());
  eval("var x = 3");
  assertEquals(27, eval_f());
}

test_non_strict_outer_eval();
