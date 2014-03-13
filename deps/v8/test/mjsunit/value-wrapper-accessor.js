// Copyright 2014 the V8 project authors. All rights reserved.
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

// When calling user-defined accessors on strings, booleans or
// numbers, we should create a wrapper object in classic-mode.

// Flags: --allow-natives-syntax

function test(object, prototype) {
  var result;
  Object.defineProperty(prototype, "nonstrict", {
    get: function() { result = this; },
    set: function(v) { result = this; }
  });
  Object.defineProperty(prototype, "strict", {
    get: function() { "use strict"; result = this; },
    set: function(v) { "use strict"; result = this; }
  });

  (function() {
    function nonstrict(s) {
      return s.nonstrict;
    }
    function strict(s) {
      return s.strict;
    }

    nonstrict(object);
    nonstrict(object);
    %OptimizeFunctionOnNextCall(nonstrict);
    result = undefined;
    nonstrict(object);
    assertEquals("object", typeof result);

    strict(object);
    strict(object);
    %OptimizeFunctionOnNextCall(strict);
    result = undefined;
    strict(object);
    assertEquals(typeof object, typeof result);
  })();

  (function() {
    function nonstrict(s) {
      return s.nonstrict = 10;
    }
    function strict(s) {
      return s.strict = 10;
    }

    nonstrict(object);
    nonstrict(object);
    %OptimizeFunctionOnNextCall(nonstrict);
    result = undefined;
    nonstrict(object);
    // TODO(1475): Support storing to primitive values.
    // This should return "object" once storing to primitive values is
    // supported.
    assertEquals("undefined", typeof result);

    strict(object);
    strict(object);
    %OptimizeFunctionOnNextCall(strict);
    result = undefined;
    strict(object);
    // TODO(1475): Support storing to primitive values.
    // This should return "object" once storing to primitive values is
    // supported.
    assertEquals("undefined", typeof result);
  })();
}

test(1, Number.prototype);
test("string", String.prototype);
test(true, Boolean.prototype);
