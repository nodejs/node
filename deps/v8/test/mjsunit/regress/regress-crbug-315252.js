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

function f(a, b, c) {
 this.a = a;
 this.b = b;
 this.c = c;
}
var o3 = new f(1, 2, 3.5);
var o4 = new f(1, 2.5, 3);
var o1 = new f(1.5, 2, 3);
var o2 = new f(1.5, 2, 3);
function migrate(o) {
 return o.a;
}
// Use migrate to stabilize o1, o2 and o4 in [double, double, smi].
migrate(o4);
migrate(o1);
migrate(o2);
function store_transition(o) {
 o.d = 1;
}
// Optimize "store_transition" to transition from [double, double, smi] to
// [double, double, smi, smi]. This adds a dependency on the
// [double, double, smi] map.
store_transition(o4);
store_transition(o1);
store_transition(o2);
%OptimizeFunctionOnNextCall(store_transition);
// Pass in a deprecated object of format [smi, smi, double]. This will migrate
// the instance, forcing a merge with [double, double, smi], ending up with
// [double, double, double], which deprecates [double, double, smi] and
// deoptimizes all dependencies of [double, double, smi], including
// store_transition itself.
store_transition(o3);
