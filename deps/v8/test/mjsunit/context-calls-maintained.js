// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc --allow-natives-syntax

function clear_all_ics() {
  %NotifyContextDisposed();
  gc();
  gc();
  gc();
}


// Test: verify that a monomorphic call retains the structural knowledge
// of a global call, correctly throwing either ReferenceError or
// TypeError on undefined depending on how the call is made.
(function() {
  foo = function(arg) { return arg + 1; }

  function f() { foo(1); }

  // Drive to monomorphic
  f(); f(); f();

  delete foo;
  assertThrows(function() { f(); }, ReferenceError);
  foo = function(arg) { return arg * 2; }
  assertDoesNotThrow(function() { f(); });
  f(); f(); f();
  delete foo;
  assertThrows(function() { f(); }, ReferenceError);
  clear_all_ics();
  foo = function(arg) { return arg * 3; }
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
  delete foo;
  assertThrows(function() { f(); }, ReferenceError);

  foo = function(arg) { return arg * 3; }
  function g() { this.foo(1); }
  g(); g(); g();
  delete foo;
  assertThrows(function() { g(); }, TypeError);
  foo = function(arg) { return arg * 3; }
  g();
  %OptimizeFunctionOnNextCall(g);
  g();
  delete foo;
  assertThrows(function() { g(); }, TypeError);
})();


// Test: verify that a load with IC does the right thing.
(function() {
  var foo = function() { return a; }
  a = 3;
  foo(); foo(); foo();
  delete a;
  assertThrows(function() { foo(); }, ReferenceError);
  a = "hi";
  foo();
  clear_all_ics();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
  delete a;
  assertThrows(function() { foo(); }, ReferenceError);
  foo = function() { return this.a; }
  assertDoesNotThrow(function() { foo(); });
})();


// Test: verify that a store with IC does the right thing.
// If store is contextual and strict mode is set, throw a ReferenceError
// if the variable isn't found.
(function() {
  var foo = function() { a = 3; }
  var bar = function() { "use strict"; a = 3; }
  foo(); foo(); foo();
  delete a;
  assertThrows(function() { bar(); }, ReferenceError);
  a = 6;
  foo(); foo(); foo();
  bar(); bar();
  clear_all_ics();
  bar();
  %OptimizeFunctionOnNextCall(bar);
  bar();
  delete a;
  assertThrows(function() { bar(); }, ReferenceError);
})();
