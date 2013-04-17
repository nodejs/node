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

// Check that the __proto__ accessor is properly poisoned when extracted
// from Object.prototype using the property descriptor.
var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertEquals("function", typeof desc.get);
assertEquals("function", typeof desc.set);
assertDoesNotThrow("desc.get.call({})");
assertThrows("desc.set.call({})", TypeError);

// Check that any redefinition of the __proto__ accessor causes poising
// to cease and the accessor to be extracted normally.
Object.defineProperty(Object.prototype, "__proto__", { get:function(){} });
desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertDoesNotThrow("desc.get.call({})");
assertThrows("desc.set.call({})", TypeError);
Object.defineProperty(Object.prototype, "__proto__", { set:function(x){} });
desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertDoesNotThrow("desc.get.call({})");
assertDoesNotThrow("desc.set.call({})");
