// Copyright 2009 the V8 project authors. All rights reserved.
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
// Testing basic functionality of the arguments object.
// Introduced to ensure that the fast compiler does the right thing.
// The arguments object itself.
assertEquals(42, function(){ return arguments;}(42)[0],
             "return arguments value");
assertEquals(42, function(){ return arguments;}(42)[0],
             "arguments in plain value context");
assertEquals(42, function(){ arguments;return 42}(37),
             "arguments in effect context");
assertEquals(42, function(){ if(arguments)return 42;}(),
             "arguments in a boolean context");
assertEquals(42, function(){ return arguments || true;}(42)[0],
             "arguments in a short-circuit boolean context - or");
assertEquals(true, function(){ return arguments && [true];}(42)[0],
             "arguments in a short-circuit boolean context - and");
assertEquals(42, function(){ arguments = 42; return 42;}(),
             "arguments assignment");
// Properties of the arguments object.
assertEquals(42, function(){ return arguments[0]; }(42),
             "args[0] value returned");
assertEquals(42, function(){ arguments[0]; return 42}(),
             "args[0] value ignored");
assertEquals(42, function(){ if (arguments[0]) return 42; }(37),
             "args[0] to boolean");
assertEquals(42, function(){ return arguments[0] || "no"; }(42),
             "args[0] short-circuit boolean or true");
assertEquals(42, function(){ return arguments[0] || 42; }(0),
             "args[0] short-circuit boolean or false");
assertEquals(37, function(){ return arguments[0] && 37; }(42),
             "args[0] short-circuit boolean and true");
assertEquals(0, function(){ return arguments[0] && 42; }(0),
             "args[0] short-circuit boolean and false");
assertEquals(42, function(){ arguments[0] = 42; return arguments[0]; }(37),
             "args[0] assignment");
// Link between arguments and parameters.
assertEquals(42, function(a) { arguments[0] = 42; return a; }(37),
             "assign args[0]->a");
assertEquals(42, function(a) { a = 42; return arguments[0]; }(37),
             "assign a->args[0]");
assertEquals(54, function(a, b) { arguments[1] = 54; return b; }(42, 37),
             "assign args[1]->b:b");
assertEquals(54, function(a, b) { b = 54; return arguments[1]; }(42, 47),
             "assign b->args[1]:b");
assertEquals(42, function(a, b) { arguments[1] = 54; return a; }(42, 37),
             "assign args[1]->b:a");
assertEquals(42, function(a, b) { b = 54; return arguments[0]; }(42, 47),
             "assign b->args[1]:a");

// Capture parameters in nested contexts.
assertEquals(33,
             function(a,b) {
                return a + arguments[0] +
                       function(b){ return a + b + arguments[0]; }(b); }(7,6),
             "captured parameters");
assertEquals(42, function(a) {
                   arguments[0] = 42;
                   return function(b){ return a; }();
             }(37),
             "capture value returned");
assertEquals(42,
             function(a) {
               arguments[0] = 26;
               return function(b){ a; return 42; }();
             }(37),
             "capture value ignored");
assertEquals(42,
             function(a) {
               arguments[0] = 26;
               return function(b){ if (a) return 42; }();
              }(37),
             "capture to boolean");
assertEquals(42,
             function(a) {
               arguments[0] = 42;
               return function(b){ return a || "no"; }();
             }(37),
             "capture short-circuit boolean or true");
assertEquals(0,
             function(a) {
               arguments[0] = 0;
               return function(b){ return a && 42; }();
             }(37),
             "capture short-circuit boolean and false");
// Deeply nested.
assertEquals(42,
             function(a,b) {
               return arguments[2] +
                      function(){
                        return b +
                               function() {
                                 return a;
                               }();
                      }();
             }(7,14,21),
             "deep nested capture");

// Assignment to captured parameters.
assertEquals(42, function(a,b) {
                   arguments[1] = 11;
                   return a + function(){ a = b; return a; }() + a;
                 }(20, 37), "captured assignment");

// Inside non-function scopes.
assertEquals(42,
             function(a) {
               arguments[0] = 20;
               with ({ b : 22 }) { return a + b; }
             }(37),
             "a in with");
assertEquals(42,
             function(a) {
               with ({ b : 22 }) { return arguments[0] + b; }
             }(20),
             "args in with");
assertEquals(42,
             function(a) {
               arguments[0] = 20;
               with ({ b : 22 }) {
                 return function() { return a; }() + b; }
             }(37),
             "captured a in with");
assertEquals(42,
             function(a) {
               arguments[0] = 12;
               with ({ b : 22 }) {
                 return function f() {
                          try { throw 8 } catch(e) { return e + a };
                         }() + b;
               }
             }(37),
             "in a catch in a named function captured a in with ");
// Escaping arguments.
function weirdargs(a,b,c) { if (!a) return arguments;
                            return [b[2],c]; }
var args1 = weirdargs(false, null, 40);
var res = weirdargs(true, args1, 15);
assertEquals(40, res[0], "return old args element");
assertEquals(15, res[1], "return own args element");
