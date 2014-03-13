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

var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertEquals("function", typeof desc.get);
assertEquals("function", typeof desc.set);
assertDoesNotThrow("desc.get.call({})");
assertDoesNotThrow("desc.set.call({}, {})");


var obj = {};
var obj2 = {};
desc.set.call(obj, obj2);
assertEquals(obj.__proto__, obj2);
assertEquals(desc.get.call(obj), obj2);


// Check that any redefinition of the __proto__ accessor works.
Object.defineProperty(Object.prototype, "__proto__", {
  get: function() {
    return 42;
  }
});
assertEquals({}.__proto__, 42);
assertEquals(desc.get.call({}), Object.prototype);


var desc2 = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertEquals(desc2.get.call({}), 42);
assertDoesNotThrow("desc2.set.call({})");


Object.defineProperty(Object.prototype, "__proto__", { set:function(x){} });
var desc3 = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertDoesNotThrow("desc3.get.call({})");
assertDoesNotThrow("desc3.set.call({})");


Object.defineProperty(Object.prototype, "__proto__", { set: undefined });
assertThrows(function() {
  "use strict";
  var o = {};
  var p = {};
  o.__proto__ = p;
}, TypeError);


assertTrue(delete Object.prototype.__proto__);
var o = {};
var p = {};
o.__proto__ = p;
assertEquals(Object.getPrototypeOf(o), Object.prototype);
var desc4 = Object.getOwnPropertyDescriptor(o, "__proto__");
assertTrue(desc4.configurable);
assertTrue(desc4.enumerable);
assertTrue(desc4.writable);
assertEquals(desc4.value, p);
