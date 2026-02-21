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

// In Issue 87, we allowed unicode escape sequences in RegExp flags.
// However, according to ES5, they should not be interpreted, but passed
// verbatim to the RegExp constructor.
// (On top of that, the original test was bugged and never tested anything).
// The behavior was changed in r8969 to not interpret escapes, but this
// test didn't test that, and only failed when making invalid flag characters
// an error too.

assertThrows("/x/\\u0067");
assertThrows("/x/\\u0069");
assertThrows("/x/\\u006d");

assertThrows("/x/\\u0067i");
assertThrows("/x/\\u0069m");
assertThrows("/x/\\u006dg");

assertThrows("/x/m\\u0067");
assertThrows("/x/g\\u0069");
assertThrows("/x/i\\u006d");

assertThrows("/x/m\\u0067i");
assertThrows("/x/g\\u0069m");
assertThrows("/x/i\\u006dg");

assertThrows("/x/\\u0068");
assertThrows("/x/\\u0020");
