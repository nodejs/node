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

// Flags: --allow-natives-syntax

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
assertEquals(275760, dMax.getFullYear());
assertEquals(8, dMax.getMonth());
assertEquals(13, dMax.getUTCDate());

var dOverflow = new Date(8.64e15+1);
assertTrue(isNaN(dOverflow.getTime()));

var dMin = new Date(-8.64e15);
assertEquals(-8.64e15, dMin.getTime());
assertEquals(-271821, dMin.getFullYear());
assertEquals(3, dMin.getMonth());
assertEquals(20, dMin.getUTCDate());

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
  var s = d.toLocaleTimeString("en-GB");
  assertEquals(8, s.length);
}

testToLocaleTimeString();

// Test that -0 is treated correctly in MakeDay.
var d = new Date();
assertDoesNotThrow("d.setDate(-0)");
assertDoesNotThrow("new Date(-0, -0, -0, -0, -0, -0, -0)");
assertDoesNotThrow("new Date(0x40000000, 0x40000000, 0x40000000," +
                   "0x40000000, 0x40000000, 0x40000000, 0x40000000)")
assertDoesNotThrow("new Date(-0x40000001, -0x40000001, -0x40000001," +
                   "-0x40000001, -0x40000001, -0x40000001, -0x40000001)")


// Modified test from WebKit
// LayoutTests/fast/js/script-tests/date-utc-timeclip.js:

assertEquals(8639999999999999, Date.UTC(275760, 8, 12, 23, 59, 59, 999));
assertEquals(8640000000000000, Date.UTC(275760, 8, 13));
assertTrue(isNaN(Date.UTC(275760, 8, 13, 0, 0, 0, 1)));
assertTrue(isNaN(Date.UTC(275760, 8, 14)));

assertEquals(Date.UTC(-271821, 3, 20, 0, 0, 0, 1), -8639999999999999);
assertEquals(Date.UTC(-271821, 3, 20), -8640000000000000);
assertTrue(isNaN(Date.UTC(-271821, 3, 19, 23, 59, 59, 999)));
assertTrue(isNaN(Date.UTC(-271821, 3, 19)));


// Test creation with large date values.
d = new Date(1969, 12, 1, 99999999999);
assertTrue(isNaN(d.getTime()));
d = new Date(1969, 12, 1, -99999999999);
assertTrue(isNaN(d.getTime()));
d = new Date(1969, 12, 1, Infinity);
assertTrue(isNaN(d.getTime()));
d = new Date(1969, 12, 1, -Infinity);
assertTrue(isNaN(d.getTime()));
d = new Date(1969, 12, 1, 0);
d.setTime(Math.pow(2, 64));
assertTrue(isNaN(d.getTime()));
d = new Date(1969, 12, 1, 0);
d.setTime(Math.pow(-2, 64));
assertTrue(isNaN(d.getTime()));


// Test creation with obscure date values.
assertEquals(8640000000000000, Date.UTC(1970, 0, 1 + 100000001, -24));
assertEquals(-8640000000000000, Date.UTC(1970, 0, 1 - 100000001, 24));


// Parsing ES5 ISO-8601 dates.
// When TZ is omitted, it defaults to 'Z' meaning UTC.

// Check epoch.
assertEquals(0, Date.parse("1970-01-01T00:00:00.000+00:00"));
assertEquals(0, Date.parse("1970-01-01T00:00:00.000-00:00"));
assertEquals(0, Date.parse("1970-01-01T00:00:00.000Z"));
assertEquals(0, Date.parse("1970-01-01T00:00:00.000"));
assertEquals(0, Date.parse("1970-01-01T00:00:00"));
assertEquals(0, Date.parse("1970-01-01T00:00"));
assertEquals(0, Date.parse("1970-01-01"));

assertEquals(0, Date.parse("1970-01T00:00:00.000+00:00"));
assertEquals(0, Date.parse("1970-01T00:00:00.000-00:00"));
assertEquals(0, Date.parse("1970-01T00:00:00.000Z"));
assertEquals(0, Date.parse("1970-01T00:00:00.000"));
assertEquals(0, Date.parse("1970-01T00:00:00"));
assertEquals(0, Date.parse("1970-01T00:00"));
assertEquals(0, Date.parse("1970-01"));

assertEquals(0, Date.parse("1970T00:00:00.000+00:00"));
assertEquals(0, Date.parse("1970T00:00:00.000-00:00"));
assertEquals(0, Date.parse("1970T00:00:00.000Z"));
assertEquals(0, Date.parse("1970T00:00:00.000"));
assertEquals(0, Date.parse("1970T00:00:00"));
assertEquals(0, Date.parse("1970T00:00"));
assertEquals(0, Date.parse("1970"));

assertEquals(0, Date.parse("+001970-01-01T00:00:00.000+00:00"));
assertEquals(0, Date.parse("+001970-01-01T00:00:00.000-00:00"));
assertEquals(0, Date.parse("+001970-01-01T00:00:00.000Z"));
assertEquals(0, Date.parse("+001970-01-01T00:00:00.000"));
assertEquals(0, Date.parse("+001970-01-01T00:00:00"));
assertEquals(0, Date.parse("+001970-01-01T00:00"));
assertEquals(0, Date.parse("+001970-01-01"));

assertEquals(0, Date.parse("+001970-01T00:00:00.000+00:00"));
assertEquals(0, Date.parse("+001970-01T00:00:00.000-00:00"));
assertEquals(0, Date.parse("+001970-01T00:00:00.000Z"));
assertEquals(0, Date.parse("+001970-01T00:00:00.000"));
assertEquals(0, Date.parse("+001970-01T00:00:00"));
assertEquals(0, Date.parse("+001970-01T00:00"));
assertEquals(0, Date.parse("+001970-01"));

assertEquals(0, Date.parse("+001970T00:00:00.000+00:00"));
assertEquals(0, Date.parse("+001970T00:00:00.000-00:00"));
assertEquals(0, Date.parse("+001970T00:00:00.000Z"));
assertEquals(0, Date.parse("+001970T00:00:00.000"));
assertEquals(0, Date.parse("+001970T00:00:00"));
assertEquals(0, Date.parse("+001970T00:00"));
assertEquals(0, Date.parse("+001970"));

// Check random date.
assertEquals(70671003500, Date.parse("1972-03-28T23:50:03.500+01:00"));
assertEquals(70674603500, Date.parse("1972-03-28T23:50:03.500Z"));
assertEquals(70674603500, Date.parse("1972-03-28T23:50:03.500"));
assertEquals(70674603000, Date.parse("1972-03-28T23:50:03"));
assertEquals(70674600000, Date.parse("1972-03-28T23:50"));
assertEquals(70588800000, Date.parse("1972-03-28"));

assertEquals(68338203500, Date.parse("1972-03T23:50:03.500+01:00"));
assertEquals(68341803500, Date.parse("1972-03T23:50:03.500Z"));
assertEquals(68341803500, Date.parse("1972-03T23:50:03.500"));
assertEquals(68341803000, Date.parse("1972-03T23:50:03"));
assertEquals(68341800000, Date.parse("1972-03T23:50"));
assertEquals(68256000000, Date.parse("1972-03"));

assertEquals(63154203500, Date.parse("1972T23:50:03.500+01:00"));
assertEquals(63157803500, Date.parse("1972T23:50:03.500Z"));
assertEquals(63157803500, Date.parse("1972T23:50:03.500"));
assertEquals(63157803000, Date.parse("1972T23:50:03"));
assertEquals(63072000000, Date.parse("1972"));

assertEquals(70671003500, Date.parse("+001972-03-28T23:50:03.500+01:00"));
assertEquals(70674603500, Date.parse("+001972-03-28T23:50:03.500Z"));
assertEquals(70674603500, Date.parse("+001972-03-28T23:50:03.500"));
assertEquals(70674603000, Date.parse("+001972-03-28T23:50:03"));
assertEquals(70674600000, Date.parse("+001972-03-28T23:50"));
assertEquals(70588800000, Date.parse("+001972-03-28"));

assertEquals(68338203500, Date.parse("+001972-03T23:50:03.500+01:00"));
assertEquals(68341803500, Date.parse("+001972-03T23:50:03.500Z"));
assertEquals(68341803500, Date.parse("+001972-03T23:50:03.500"));
assertEquals(68341803000, Date.parse("+001972-03T23:50:03"));
assertEquals(68341800000, Date.parse("+001972-03T23:50"));
assertEquals(68256000000, Date.parse("+001972-03"));

assertEquals(63154203500, Date.parse("+001972T23:50:03.500+01:00"));
assertEquals(63157803500, Date.parse("+001972T23:50:03.500Z"));
assertEquals(63157803500, Date.parse("+001972T23:50:03.500"));
assertEquals(63157803000, Date.parse("+001972T23:50:03"));
assertEquals(63072000000, Date.parse("+001972"));


// Ensure that ISO-years in the range 00-99 aren't translated to the range
// 1950..2049.
assertEquals(-60904915200000, Date.parse("0040-01-01"));
assertEquals(-60273763200000, Date.parse("0060-01-01"));
assertEquals(-62167219200000, Date.parse("0000-01-01"));
assertEquals(-62167219200000, Date.parse("+000000-01-01"));

// Test negative years.
assertEquals(-63429523200000, Date.parse("-000040-01-01"));
assertEquals(-64060675200000, Date.parse("-000060-01-01"));
assertEquals(-124397510400000, Date.parse("-001972-01-01"));

// Check time-zones.
assertEquals(70674603500, Date.parse("1972-03-28T23:50:03.500Z"));
for (var i = 0; i < 24; i++) {
  var hh = (i < 10) ? "0" + i : "" + i;
  for (var j = 0; j < 60; j += 15) {
    var mm = (j < 10) ? "0" + j : "" + j;
    var ms = (i * 60 + j) * 60000;
    var string = "1972-03-28T23:50:03.500-" + hh + ":" + mm;
    assertEquals(70674603500 + ms, Date.parse(string), string);
    string = "1972-03-28T23:50:03.500+" + hh + ":" + mm;
    assertEquals(70674603500 - ms, Date.parse(string), string);
  }
}

assertThrows('Date.prototype.setTime.call("", 1);', TypeError);
assertThrows('Date.prototype.setYear.call("", 1);', TypeError);
assertThrows('Date.prototype.setHours.call("", 1, 2, 3, 4);', TypeError);
assertThrows('Date.prototype.getDate.call("");', TypeError);
assertThrows('Date.prototype.getUTCDate.call("");', TypeError);

var date = new Date();
date.getTime();
date.getTime();
%OptimizeFunctionOnNextCall(Date.prototype.getTime);
assertThrows(function() { Date.prototype.getTime.call(""); }, TypeError);
assertUnoptimized(Date.prototype.getTime);

date.getYear();
date.getYear();
%OptimizeFunctionOnNextCall(Date.prototype.getYear);
assertThrows(function() { Date.prototype.getYear.call(""); }, TypeError);
assertUnoptimized(Date.prototype.getYear);
