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

// Test date construction from other dates.
var date0 = new Date(1111);
var date1 = new Date(date0);
assertEquals(1111, date0.getTime());
assertEquals(date0.getTime(), date1.getTime());
var date2 = new Date(date0.toString());
assertEquals(1000, date2.getTime());

// Test that dates may contain commas.
var date0 = Date.parse("Dec 25 1995 1:30");
var date1 = Date.parse("Dec 25, 1995 1:30");
var date2 = Date.parse("Dec 25 1995, 1:30");
var date3 = Date.parse("Dec 25, 1995, 1:30");
assertEquals(date0, date1);
assertEquals(date1, date2);
assertEquals(date2, date3);

// Test limits (+/-1e8 days from epoch)

var dMax = new Date(8.64e15);
assertEquals(8.64e15, dMax.getTime());

var dOverflow = new Date(8.64e15+1);
assertTrue(isNaN(dOverflow.getTime()));

var dMin = new Date(-8.64e15);
assertEquals(-8.64e15, dMin.getTime());

var dUnderflow = new Date(-8.64e15-1);
assertTrue(isNaN(dUnderflow.getTime()));


// Tests inspired by js1_5/Date/regress-346363.js

// Year
var a = new Date();
a.setFullYear();
a.setFullYear(2006);
assertEquals(2006, a.getFullYear());

var b = new Date();
b.setUTCFullYear();
b.setUTCFullYear(2006);
assertEquals(2006, b.getUTCFullYear());

// Month
var c = new Date();
c.setMonth();
c.setMonth(2);
assertTrue(isNaN(c.getMonth()));

var d = new Date();
d.setUTCMonth();
d.setUTCMonth(2);
assertTrue(isNaN(d.getUTCMonth()));

// Date
var e = new Date();
e.setDate();
e.setDate(2);
assertTrue(isNaN(e.getDate()));

var f = new Date();
f.setUTCDate();
f.setUTCDate(2);
assertTrue(isNaN(f.getUTCDate()));

// Hours
var g = new Date();
g.setHours();
g.setHours(2);
assertTrue(isNaN(g.getHours()));

var h = new Date();
h.setUTCHours();
h.setUTCHours(2);
assertTrue(isNaN(h.getUTCHours()));

// Minutes
var g = new Date();
g.setMinutes();
g.setMinutes(2);
assertTrue(isNaN(g.getMinutes()));

var h = new Date();
h.setUTCHours();
h.setUTCHours(2);
assertTrue(isNaN(h.getUTCHours()));


// Seconds
var i = new Date();
i.setSeconds();
i.setSeconds(2);
assertTrue(isNaN(i.getSeconds()));

var j = new Date();
j.setUTCSeconds();
j.setUTCSeconds(2);
assertTrue(isNaN(j.getUTCSeconds()));


// Milliseconds
var k = new Date();
k.setMilliseconds();
k.setMilliseconds(2);
assertTrue(isNaN(k.getMilliseconds()));

var l = new Date();
l.setUTCMilliseconds();
l.setUTCMilliseconds(2);
assertTrue(isNaN(l.getUTCMilliseconds()));

// Test that toLocaleTimeString only returns the time portion of the
// date without the timezone information.
function testToLocaleTimeString() {
  var d = new Date();
  var s = d.toLocaleTimeString();
  assertEquals(8, s.length);
}

testToLocaleTimeString();


// Modified test from WebKit
// LayoutTests/fast/js/script-tests/date-utc-timeclip.js:

assertEquals(Date.UTC(275760, 8, 12, 23, 59, 59, 999), 8639999999999999);
assertEquals(Date.UTC(275760, 8, 13), 8640000000000000);
assertTrue(isNaN(Date.UTC(275760, 8, 13, 0, 0, 0, 1)));
assertTrue(isNaN(Date.UTC(275760, 8, 14)));

assertEquals(Date.UTC(-271821, 3, 20, 0, 0, 0, 1), -8639999999999999);
assertEquals(Date.UTC(-271821, 3, 20), -8640000000000000);
assertTrue(isNaN(Date.UTC(-271821, 3, 19, 23, 59, 59, 999)));
assertTrue(isNaN(Date.UTC(-271821, 3, 19)));
