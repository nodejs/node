// Copyright 2010 the V8 project authors. All rights reserved.
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

// Check for parsing of proper ES5 15.9.1.15 (ISO 8601 / RFC 3339) time
// strings that contain millisecond values with exactly 3 digits (as is
// required by the spec format if the string has milliseconds at all).
assertEquals(1290722550521, Date.parse("2010-11-25T22:02:30.521Z"));

// Check for parsing of extension/generalization of the ES5 15.9.1.15 spec
// format where millisecond values have only 1 or 2 digits.
assertEquals(1290722550500, Date.parse("2010-11-25T22:02:30.5Z"));
assertEquals(1290722550520, Date.parse("2010-11-25T22:02:30.52Z"));
assertFalse(Date.parse("2010-11-25T22:02:30.5Z") === Date.parse("2010-11-25T22:02:30.005Z"));

// Check that we truncate millisecond values having more than 3 digits.
assertEquals(Date.parse("2010-11-25T22:02:30.1005Z"), Date.parse("2010-11-25T22:02:30.100Z"));

// Check that we accept lots of digits.
assertEquals(Date.parse("2010-11-25T22:02:30.999Z"), Date.parse("2010-11-25T22:02:30.99999999999999999999999999999999999999999999999999999999999999999999999999999999999999Z"));

// Fail if there's a decimal point but zero digits for (expected) milliseconds.
assertTrue(isNaN(Date.parse("2010-11-25T22:02:30.Z")));
