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

a = [1];
Object.defineProperty(a, "1", {writable:false, configurable:false, value: 100});
assertThrows("a.unshift(4);", TypeError);
assertEquals([1, 100, 100], a);
var desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.writable);
assertEquals(false, desc.configurable);

a = [1];
var g = function() { return 100; };
Object.defineProperty(a, "1", {get:g});
assertThrows("a.unshift(0);", TypeError);
assertEquals([1, 100, 100], a);
desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.configurable);
assertEquals(g, desc.get);

a = [1];
var c = 0;
var s = function(v) { c += 1; };
Object.defineProperty(a, "1", {set:s});
a.unshift(10);
assertEquals([10, undefined, undefined], a);
assertEquals(1, c);
desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.configurable);
assertEquals(s, desc.set);

a = [1];
Object.defineProperty(a, "1", {configurable:false, value:10});
assertThrows("a.splice(1,1);", TypeError);
assertEquals([1, 10], a);
desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.configurable);

a = [0,1,2,3,4,5,6];
Object.defineProperty(a, "3", {configurable:false, writable:false, value:3});
assertThrows("a.splice(1,4);", TypeError);
assertEquals([0,5,6,3,,,,], a);
desc = Object.getOwnPropertyDescriptor(a, "3");
assertEquals(false, desc.configurable);
assertEquals(false, desc.writable);

a = [0,1,2,3,4,5,6];
Object.defineProperty(a, "5", {configurable:false, value:5});
assertThrows("a.splice(1,4);", TypeError);
assertEquals([0,5,6,3,4,5,,], a);
desc = Object.getOwnPropertyDescriptor(a, "5");
assertEquals(false, desc.configurable);

a = [1,2,3,,5];
Object.defineProperty(a, "1", {configurable:false, writable:true, value:2});
assertEquals(1, a.shift());
assertEquals([2,3,,5], a);
desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.configurable);
assertEquals(true, desc.writable);
assertThrows("a.shift();", TypeError);
assertEquals([3,3,,5], a);
desc = Object.getOwnPropertyDescriptor(a, "1");
assertEquals(false, desc.configurable);
assertEquals(true, desc.writable);

a = [1,2,3];
Object.defineProperty(a, "2", {configurable:false, value:3});
assertThrows("a.pop();", TypeError);
assertEquals([1,2,3], a);
desc = Object.getOwnPropertyDescriptor(a, "2");
assertEquals(false, desc.configurable);
