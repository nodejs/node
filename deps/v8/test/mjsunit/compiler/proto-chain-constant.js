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

// Test loading a constant function on the prototype chain.

var c = Object.create;
var obj4 = c(null, { f4: { value: function() { return 4; }, writable: true }});
var obj3 = c(obj4, { f3: { value: function() { return 3; }, writable: true }});
var obj2 = c(obj3, { f2: { value: function() { return 2; }, writable: true }});
var obj1 = c(obj2, { f1: { value: function() { return 1; }, writable: true }});
var obj0 = c(obj1, { f0: { value: function() { return 0; }, writable: true }});

function get4(obj) { return obj.f4; }

assertEquals(4, get4(obj0)());
assertEquals(4, get4(obj0)());
%OptimizeFunctionOnNextCall(get4);
assertEquals(4, get4(obj0)());
obj4.f4 = function() { return 5; };
assertEquals(5, get4(obj0)());

function get3(obj) { return obj.f3; }

assertEquals(3, get3(obj0)());
assertEquals(3, get3(obj0)());
%OptimizeFunctionOnNextCall(get3);
assertEquals(3, get3(obj0)());
obj2.f3 = function() { return 6; };
assertEquals(6, get3(obj0)());
