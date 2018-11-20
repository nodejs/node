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

// Flags: --allow-natives-syntax --nodead-code-elimination
// Flags: --nofold-constants --nouse-gvn

// Create a function to get a long series of removable simulates.
// f() {
//   var _0 = <random>, _1 = <random>, ... _1000 = <random>,
//   _1001 = <random var> + <random var>,
//   _1002 = <random var> + <random var>,
//   ...
//   _99999 = <random var> + <random var>,
//   x = 1;
//   return _0;
// }

var seed = 1;

function rand() {
  seed = seed * 171 % 1337 + 17;
  return (seed % 1000) / 1000;
}

function randi(max) {
  seed = seed * 131 % 1773 + 13;
  return seed % max;
}

function varname(i) {
  return "_" + i;
}

var source = "var ";

for (var i = 0; i < 750; i++) {
  source += [varname(i), "=", rand(), ","].join("");
}

for (var i = 750; i < 3000; i++) {
  source += [varname(i), "=",
             varname(randi(i)), "+",
             varname(randi(i)), ","].join("");
}

source += "x=1; return _0;"
var f = new Function(source);

f();
%OptimizeFunctionOnNextCall(f);
f();
