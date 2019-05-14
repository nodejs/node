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

// Flags: --allow-natives-syntax

var o1 = {x:1};
var o2 = {};
var deopt_getter = false;
var deopt_setter = false;

function f_mono(o) {
  return 5 + o.x++;
}

var to_deopt = f_mono;

var v = 1;
var g = 0;
var s = 0;

Object.defineProperty(o2, "x",
    {get:function() {
       g++;
       if (deopt_getter) {
         deopt_getter = false;
         %DeoptimizeFunction(to_deopt);
       }
       return v;
     },
     set:function(new_v) {
       v = new_v;
       s++;
       if (deopt_setter) {
         deopt_setter = false;
         %DeoptimizeFunction(to_deopt);
       }
     }});

assertEquals(6, f_mono(o2));
assertEquals(1, g);
assertEquals(1, s);
assertEquals(7, f_mono(o2));
assertEquals(2, g);
assertEquals(2, s);
%OptimizeFunctionOnNextCall(f_mono);
deopt_setter = true;
assertEquals(8, f_mono(o2));
assertEquals(3, g);
assertEquals(3, s);

function f_poly(o) {
  return 5 + o.x++;
}

v = 1;
to_deopt = f_poly;

f_poly(o1);
f_poly(o1);
assertEquals(6, f_poly(o2));
assertEquals(4, g);
assertEquals(4, s);
assertEquals(7, f_poly(o2));
assertEquals(5, g);
assertEquals(5, s);
%OptimizeFunctionOnNextCall(f_poly);
deopt_setter = true;
assertEquals(8, f_poly(o2));
assertEquals(6, g);
assertEquals(6, s);

%OptimizeFunctionOnNextCall(f_poly);
v = undefined;
assertEquals(NaN, f_poly(o2));
assertEquals(7, g);
assertEquals(7, s);

function f_pre(o) {
  return 5 + ++o.x;
}

v = 1;
to_deopt = f_pre;

f_pre(o1);
f_pre(o1);
assertEquals(7, f_pre(o2));
assertEquals(8, g);
assertEquals(8, s);
assertEquals(8, f_pre(o2));
assertEquals(9, g);
assertEquals(9, s);
%OptimizeFunctionOnNextCall(f_pre);
deopt_setter = true;
assertEquals(9, f_pre(o2));
assertEquals(10, g);
assertEquals(10, s);

%OptimizeFunctionOnNextCall(f_pre);
v = undefined;
assertEquals(NaN, f_pre(o2));
assertEquals(11, g);
assertEquals(11, s);


function f_get(o) {
  return 5 + o.x++;
}

v = 1;
to_deopt = f_get;

f_get(o1);
f_get(o1);
assertEquals(6, f_get(o2));
assertEquals(12, g);
assertEquals(12, s);
assertEquals(7, f_get(o2));
assertEquals(13, g);
assertEquals(13, s);
%OptimizeFunctionOnNextCall(f_get);
deopt_getter = true;
assertEquals(8, f_get(o2));
assertEquals(14, g);
assertEquals(14, s);

%OptimizeFunctionOnNextCall(f_get);
v = undefined;
assertEquals(NaN, f_get(o2));
assertEquals(15, g);
assertEquals(15, s);
