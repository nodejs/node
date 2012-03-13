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

// Test Harmony Number.isFinite() and Number.isNaN() functions.

assertTrue(Number.isFinite(0));
assertTrue(Number.isFinite(Number.MIN_VALUE));
assertTrue(Number.isFinite(Number.MAX_VALUE));
assertFalse(Number.isFinite(Number.NaN));
assertFalse(Number.isFinite(Number.POSITIVE_INFINITY));
assertFalse(Number.isFinite(Number.NEGATIVE_INFINITY));
assertFalse(Number.isFinite(new Number(0)));
assertFalse(Number.isFinite(1/0));
assertFalse(Number.isFinite(-1/0));
assertFalse(Number.isFinite({}));
assertFalse(Number.isFinite([]));
assertFalse(Number.isFinite("s"));
assertFalse(Number.isFinite(null));
assertFalse(Number.isFinite(undefined));

assertFalse(Number.isNaN(0));
assertFalse(Number.isNaN(Number.MIN_VALUE));
assertFalse(Number.isNaN(Number.MAX_VALUE));
assertTrue(Number.isNaN(Number.NaN));
assertFalse(Number.isNaN(Number.POSITIVE_INFINITY));
assertFalse(Number.isNaN(Number.NEGATIVE_INFINITY));
assertFalse(Number.isNaN(new Number(0)));
assertFalse(Number.isNaN(1/0));
assertFalse(Number.isNaN(-1/0));
assertFalse(Number.isNaN({}));
assertFalse(Number.isNaN([]));
assertFalse(Number.isNaN("s"));
assertFalse(Number.isNaN(null));
assertFalse(Number.isNaN(undefined));
