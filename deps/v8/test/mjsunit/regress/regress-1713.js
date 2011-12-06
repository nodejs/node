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

// Flags: --allow-natives-syntax --always-compact --expose-gc

var O = { get f() { return 0; } };

var CODE = [];

var R = [];

function Allocate4Kb(N) {
  var arr = [];
  do {arr.push(new Array(1024));} while (--N > 0);
  return arr;
}

function AllocateXMb(X) {
  return Allocate4Kb((1024 * X) / 4);
}

function Node(v, next) { this.v = v; this.next = next; }

Node.prototype.execute = function (O) {
  var n = this;
  while (n.next !== null) n = n.next;
  n.v(O);
};

function LongList(N, x) {
  if (N == 0) return new Node(x, null);
  return new Node(new Array(1024), LongList(N - 1, x));
}

var L = LongList(1024, function (O) {
  for (var i = 0; i < 5; i++) O.f;
});



function Incremental(O, x) {
  if (!x) {
    return;
  }
  function CreateCode(i) {
    var f = new Function("return O.f_" + i);
    CODE.push(f);
    f(); // compile
    f(); // compile
    f(); // compile
  }

  for (var i = 0; i < 1e4; i++) CreateCode(i);
  gc();
  gc();
  gc();

  print(">>> 1 <<<");

  L.execute(O);

  try {} catch (e) {}

  L = null;
  print(">>> 2 <<<");
  AllocateXMb(8);
 //rint("1");
 //llocateXMb(8);
 //rint("1");
 //llocateXMb(8);

}

function foo(O, x) {
  Incremental(O, x);

  print('f');

  for (var i = 0; i < 5; i++) O.f;


  print('g');

  bar(x);
}

function bar(x) {
  if (!x) return;
  %DeoptimizeFunction(foo);
  AllocateXMb(8);
  AllocateXMb(8);
}

var O1 = {};
var O2 = {};
var O3 = {};
var O4 = {f:0};

foo(O1, false);
foo(O2, false);
foo(O3, false);
%OptimizeFunctionOnNextCall(foo);
foo(O4, true);
