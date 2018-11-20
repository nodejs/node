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

// Check baseline for __proto__.
var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertFalse(desc.enumerable);
assertTrue(desc.configurable);
assertEquals("function", typeof desc.get);
assertEquals("function", typeof desc.set);

// Check redefining getter for __proto__.
function replaced_get() {};
Object.defineProperty(Object.prototype, "__proto__", { get:replaced_get });
desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertFalse(desc.enumerable);
assertTrue(desc.configurable);
assertSame(replaced_get, desc.get);

// Check redefining setter for __proto__.
function replaced_set(x) {};
Object.defineProperty(Object.prototype, "__proto__", { set:replaced_set });
desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertFalse(desc.enumerable);
assertTrue(desc.configurable);
assertSame(replaced_set, desc.set);

// Check changing configurability of __proto__.
Object.defineProperty(Object.prototype, "__proto__", { configurable:false });
desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertFalse(desc.enumerable);
assertFalse(desc.configurable);
assertSame(replaced_get, desc.get);
assertSame(replaced_set, desc.set);

// Check freezing Object.prototype completely.
Object.freeze(Object.prototype);
assertTrue(Object.isFrozen(Object.prototype));
