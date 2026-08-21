// Copyright 2008 the V8 project authors. All rights reserved.
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

// Test the exponential notation output.
assertEquals("1e+27", (1.2345e+27).toPrecision(1));
assertEquals("1.2e+27", (1.2345e+27).toPrecision(2));
assertEquals("1.23e+27", (1.2345e+27).toPrecision(3));
assertEquals("1.234e+27", (1.2345e+27).toPrecision(4));
assertEquals("1.2345e+27", (1.2345e+27).toPrecision(5));
assertEquals("1.23450e+27", (1.2345e+27).toPrecision(6));
assertEquals("1.234500e+27", (1.2345e+27).toPrecision(7));

assertEquals("-1e+27", (-1.2345e+27).toPrecision(1));
assertEquals("-1.2e+27", (-1.2345e+27).toPrecision(2));
assertEquals("-1.23e+27", (-1.2345e+27).toPrecision(3));
assertEquals("-1.234e+27", (-1.2345e+27).toPrecision(4));
assertEquals("-1.2345e+27", (-1.2345e+27).toPrecision(5));
assertEquals("-1.23450e+27", (-1.2345e+27).toPrecision(6));
assertEquals("-1.234500e+27", (-1.2345e+27).toPrecision(7));


// Test the fixed notation output.
assertEquals("7", (7).toPrecision(1));
assertEquals("7.0", (7).toPrecision(2));
assertEquals("7.00", (7).toPrecision(3));

assertEquals("-7", (-7).toPrecision(1));
assertEquals("-7.0", (-7).toPrecision(2));
assertEquals("-7.00", (-7).toPrecision(3));

assertEquals("9e+1", (91).toPrecision(1));
assertEquals("91", (91).toPrecision(2));
assertEquals("91.0", (91).toPrecision(3));
assertEquals("91.00", (91).toPrecision(4));

assertEquals("-9e+1", (-91).toPrecision(1));
assertEquals("-91", (-91).toPrecision(2));
assertEquals("-91.0", (-91).toPrecision(3));
assertEquals("-91.00", (-91).toPrecision(4));

assertEquals("9e+1", (91.1234).toPrecision(1));
assertEquals("91", (91.1234).toPrecision(2));
assertEquals("91.1", (91.1234).toPrecision(3));
assertEquals("91.12", (91.1234).toPrecision(4));
assertEquals("91.123", (91.1234).toPrecision(5));
assertEquals("91.1234", (91.1234).toPrecision(6));
assertEquals("91.12340", (91.1234).toPrecision(7));
assertEquals("91.123400", (91.1234).toPrecision(8));

assertEquals("-9e+1", (-91.1234).toPrecision(1));
assertEquals("-91", (-91.1234).toPrecision(2));
assertEquals("-91.1", (-91.1234).toPrecision(3));
assertEquals("-91.12", (-91.1234).toPrecision(4));
assertEquals("-91.123", (-91.1234).toPrecision(5));
assertEquals("-91.1234", (-91.1234).toPrecision(6));
assertEquals("-91.12340", (-91.1234).toPrecision(7));
assertEquals("-91.123400", (-91.1234).toPrecision(8));
