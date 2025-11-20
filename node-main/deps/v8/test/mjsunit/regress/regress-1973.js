// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test that getters and setters pass unwrapped this values in strict mode
// and wrapped this values is non-strict mode.

function TestAccessorWrapping(primitive) {
  var prototype = Object.getPrototypeOf(Object(primitive))
  // Check that strict mode passes unwrapped this value.
  var strict_type = typeof primitive;
  Object.defineProperty(prototype, "strict", {
    get: function() { "use strict"; assertSame(strict_type, typeof this); },
    set: function() { "use strict"; assertSame(strict_type, typeof this); }
  });
  primitive.strict = primitive.strict;
  // Check that non-strict mode passes wrapped this value.
  var sloppy_type = typeof Object(primitive);
  Object.defineProperty(prototype, "sloppy", {
    get: function() { assertSame(sloppy_type, typeof this); },
    set: function() { assertSame(sloppy_type, typeof this); }
  });
  primitive.sloppy = primitive.sloppy;
}

TestAccessorWrapping(true);
TestAccessorWrapping(0);
TestAccessorWrapping({});
TestAccessorWrapping("");
