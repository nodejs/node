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

// Flags: --allow-natives-syntax --check-elimination


function test_empty() {
  function foo(o) {
    return { value: o.value };
  }

  function Base() {
    this.v_ = 5;
  }
  Base.prototype.__defineGetter__("value", function() { return 1; });

  var a = new Base();
  a.a = 1;
  foo(a);

  Base.prototype.__defineGetter__("value", function() { return this.v_; });

  var b = new Base();
  b.b = 1;
  foo(b);

  var d = new Base();
  d.d = 1;
  d.value;

  %OptimizeFunctionOnNextCall(foo);

  var o = foo(b);
}


function test_narrow1() {
  function foo(o) {
    return { value: o.value };
  }

  function Base() {
    this.v_ = 5;
  }
  Base.prototype.__defineGetter__("value", function() { return 1; });

  var a = new Base();
  a.a = 1;
  foo(a);

  Base.prototype.__defineGetter__("value", function() { return this.v_; });

  var b = new Base();
  b.b = 1;
  foo(b);

  var c = new Base();
  c.c = 1;
  foo(c);

  var d = new Base();
  d.d = 1;
  d.value;

  %OptimizeFunctionOnNextCall(foo);

  var o = foo(b);
}


function test_narrow2() {
  function foo(o, flag) {
    return { value: o.value(flag) };
  }

  function Base() {
    this.v_ = 5;
  }
  Base.prototype.value = function(flag) { return flag ? this.v_ : this.v_; };


  var a = new Base();
  a.a = 1;
  foo(a, false);
  foo(a, false);

  var b = new Base();
  b.b = 1;
  foo(b, true);

  var c = new Base();
  c.c = 1;
  foo(c, true);

  var d = new Base();
  d.d = 1;
  d.value(true);

  %OptimizeFunctionOnNextCall(foo);

  var o = foo(b);
}

test_empty();
test_narrow1();
test_narrow2();
