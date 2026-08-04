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

// Check what we do if toLocaleString doesn't return a string when we are
// calling Array.prototype.toLocaleString.  The standard is somewhat
// vague on this point.  This test is now passed by both V8 and JSC.

var evil_called = 0;
var evil_locale_called = 0;
var exception_thrown = 0;

function evil_to_string() {
  evil_called++;
  return this;
}

function evil_to_locale_string() {
  evil_locale_called++;
  return this;
}

var o = {toString: evil_to_string, toLocaleString: evil_to_locale_string};

try {
  [o].toLocaleString();
} catch(e) {
  exception_thrown++;
}

assertEquals(1, evil_called, "evil1");
assertEquals(1, evil_locale_called, "local1");
assertEquals(1, exception_thrown, "exception1");

try {
  [o].toString();
} catch(e) {
  exception_thrown++;
}

assertEquals(2, evil_called, "evil2");
assertEquals(1, evil_locale_called, "local2");
assertEquals(2, exception_thrown, "exception2");

try {
  [o].join(o);
} catch(e) {
  exception_thrown++;
}

assertEquals(3, evil_called, "evil3");
assertEquals(1, evil_locale_called, "local3");
assertEquals(3, exception_thrown, "exception3");
print("ok");
