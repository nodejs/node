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

// Flags: --es5_readonly

// According to ECMA-262, sections 8.6.2.2 and 8.6.2.3 you're not
// allowed to override read-only properties, not even if the read-only
// property is in the prototype chain.
//
// However, for compatibility with WebKit/JSC, we allow the overriding
// of read-only properties in prototype chains.

function F() {};
F.prototype = Number;

var original_number_max = Number.MAX_VALUE;

// Assignment to a property which does not exist on the object itself,
// but is read-only in a prototype does not take effect.
var f = new F();
assertEquals(original_number_max, f.MAX_VALUE);
f.MAX_VALUE = 42;
assertEquals(original_number_max, f.MAX_VALUE);

// Assignment to a property which does not exist on the object itself,
// but is read-only in a prototype does not take effect.
f = new F();
with (f) {
  MAX_VALUE = 42;
}
assertEquals(original_number_max, f.MAX_VALUE);

// Assignment to read-only property on the object itself is ignored.
Number.MAX_VALUE = 42;
assertEquals(original_number_max, Number.MAX_VALUE);

// G should be read-only on the global object and the assignment is
// ignored.
(function G() {
  eval("G = 42;");
  assertTrue(typeof G === 'function');
})();
