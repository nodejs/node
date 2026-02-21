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

var CheckStringReceiver = function() {
  "use strict";
  // Receivers of strict functions are not coerced.
  assertEquals("string", typeof this);
};

var CheckNumberReceiver = function() {
  "use strict";
  // Receivers of strict functions are not coerced.
  assertEquals("number", typeof this);
};

var CheckUndefinedReceiver = function() {
  "use strict";
  // Receivers of strict functions are not coerced.
  assertEquals("undefined", String(this));
};

var CheckNullReceiver = function() {
  "use strict";
  // Receivers of strict functions are not coerced.
  assertEquals("null", String(this));
};

var CheckCoersion = function() {
  // Receivers of non-strict functions are coerced to objects.
  assertEquals("object", typeof this);
};


function strict_mode() {
  "use strict";
  CheckStringReceiver.call("foo");
  CheckNumberReceiver.call(42);
  CheckUndefinedReceiver.call(undefined);
  CheckNullReceiver.call(null);
  [1].forEach(CheckStringReceiver, "foo");
  [2].every(CheckStringReceiver, "foo");
  [3].filter(CheckStringReceiver, "foo");
  [4].some(CheckNumberReceiver, 42);
  [5].map(CheckNumberReceiver, 42);

  CheckCoersion.call("foo");
  CheckCoersion.call(42);
  CheckCoersion.call(undefined);
  CheckCoersion.call(null);
  [1].forEach(CheckCoersion, "foo");
  [2].every(CheckCoersion, "foo");
  [3].filter(CheckCoersion, "foo");
  [4].some(CheckCoersion, 42);
  [5].map(CheckCoersion, 42);
};
strict_mode();

function sloppy_mode() {
  CheckStringReceiver.call("foo");
  CheckNumberReceiver.call(42);
  CheckUndefinedReceiver.call(undefined);
  CheckNullReceiver.call(null);
  [1].forEach(CheckStringReceiver, "foo");
  [2].every(CheckStringReceiver, "foo");
  [3].filter(CheckStringReceiver, "foo");
  [4].some(CheckNumberReceiver, 42);
  [5].map(CheckNumberReceiver, 42);

  CheckCoersion.call("foo");
  CheckCoersion.call(42);
  CheckCoersion.call(undefined);
  CheckCoersion.call(null);
  [1].forEach(CheckCoersion, "foo");
  [2].every(CheckCoersion, "foo");
  [3].filter(CheckCoersion, "foo");
  [4].some(CheckCoersion, 42);
  [5].map(CheckCoersion, 42);
};
sloppy_mode();
