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

// See: http://code.google.com/p/v8/issues/detail?id=485

// Ensure that we don't expose the builtins object when calling
// builtin functions that use or return "this".

var global = this;
var global2 = (function(){return this;})();
assertEquals(global, global2, "direct call to local function returns global");

var builtin = Object.prototype.valueOf;  // Builtin function that returns this.

assertEquals(global, builtin(), "Direct call to builtin");

assertEquals(global, builtin.call(), "call() to builtin");
assertEquals(global, builtin.call(null), "call(null) to builtin");
assertEquals(global, builtin.call(undefined), "call(undefined) to builtin");

assertEquals(global, builtin.apply(), "apply() to builtin");
assertEquals(global, builtin.apply(null), "apply(null) to builtin");
assertEquals(global, builtin.apply(undefined), "apply(undefined) to builtin");

assertEquals(global, builtin.call.call(builtin), "call.call() to builtin");
assertEquals(global, builtin.call.apply(builtin), "call.apply() to builtin");
assertEquals(global, builtin.apply.call(builtin), "apply.call() to builtin");
assertEquals(global, builtin.apply.apply(builtin), "apply.apply() to builtin");


// Builtin that depends on value of this to compute result.
var builtin2 = Object.prototype.toString;

// Global object has class "Object" according to Object.prototype.toString.
// Builtins object displays as "[object builtins]".
assertTrue("[object builtins]" != builtin2(), "Direct call to toString");
assertTrue("[object builtins]" != builtin2.call(), "call() to toString");
assertTrue("[object builtins]" != builtin2.apply(), "call() to toString");
assertTrue("[object builtins]" != builtin2.call.call(builtin2),
           "call.call() to toString");
