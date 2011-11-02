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

var x = "";
var v = new Object();
var w = new Object();
var vv = function() { x += "hest"; return 1; }
var ww = function() { x += "fisk"; return 2; }
v.valueOf = vv;
w.valueOf = ww;
assertEquals(1, Math.min(v,w));
assertEquals("hestfisk", x, "min");

x = "";
assertEquals(2, Math.max(v,w));
assertEquals("hestfisk", x, "max");

x = "";
assertEquals(1, Math.max(v,v));
assertEquals("hesthest", x, "max_identical");

x = "";
assertEquals(2, Math.min(w,w));
assertEquals("fiskfisk", x, "max");

x = "";
assertEquals(Math.atan2(1, 2), Math.atan2(v, w));
// JSC says fiskhest.
assertEquals("hestfisk", x, "atan2");

x = "";
assertEquals(1, Math.pow(v, w));
assertEquals("hestfisk", x, "pow");

var year = { valueOf: function() { x += 1; return 2007; } };
var month = { valueOf: function() { x += 2; return 2; } };
var date = { valueOf: function() { x += 3; return 4; } };
var hours = { valueOf: function() { x += 4; return 13; } };
var minutes = { valueOf: function() { x += 5; return 50; } };
var seconds = { valueOf: function() { x += 6; return 0; } };
var ms = { valueOf: function() { x += 7; return 999; } };

x = "";
new Date(year, month, date, hours, minutes, seconds, ms);
// JSC fails this one: Returns 12345671234567.
assertEquals("1234567", x, "Date");

x = "";
Date(year, month, date, hours, minutes, seconds, ms);
assertEquals("", x, "Date not constructor");

x = "";
Date.UTC(year, month, date, hours, minutes, seconds, ms);
// JSC fails this one: Returns 12345671234567.
assertEquals("1234567", x, "Date.UTC");

x = "";
new Date().setSeconds(seconds, ms);
assertEquals("67", x, "Date.UTC");

x = "";
new Date().setSeconds(seconds, ms);
assertEquals("67", x, "Date.setSeconds");

x = "";
new Date().setUTCSeconds(seconds, ms);
assertEquals("67", x, "Date.setUTCSeconds");

x = "";
new Date().setMinutes(minutes, seconds, ms);
assertEquals("567", x, "Date.setMinutes");

x = "";
new Date().setUTCMinutes(minutes, seconds, ms);
assertEquals("567", x, "Date.setUTCMinutes");

x = "";
new Date().setHours(hours, minutes, seconds, ms);
assertEquals("4567", x, "Date.setHours");

x = "";
new Date().setUTCHours(hours, minutes, seconds, ms);
assertEquals("4567", x, "Date.setUTCHours");

x = "";
new Date().setDate(date, hours, minutes, seconds, ms);
assertEquals("3", x, "Date.setDate");

x = "";
new Date().setUTCDate(date, hours, minutes, seconds, ms);
assertEquals("3", x, "Date.setUTCDate");

x = "";
new Date().setMonth(month, date, hours, minutes, seconds, ms);
assertEquals("23", x, "Date.setMonth");

x = "";
new Date().setUTCMonth(month, date, hours, minutes, seconds, ms);
assertEquals("23", x, "Date.setUTCMonth");

x = "";
new Date().setFullYear(year, month, date, hours, minutes, seconds, ms);
assertEquals("123", x, "Date.setFullYear");

x = "";
new Date().setUTCFullYear(year, month, date, hours, minutes, seconds, ms);
assertEquals("123", x, "Date.setUTCFullYear");

x = "";
var a = { valueOf: function() { x += "hest"; return 97; } };
var b = { valueOf: function() { x += "fisk"; return 98; } };
assertEquals("ab", String.fromCharCode(a, b), "String.fromCharCode");
assertEquals("hestfisk", x, "String.fromCharCode valueOf order");



// Test whether valueOf is called when comparing identical objects
x = "";
assertTrue(a < b, "Compare objects a < b");
assertEquals("hestfisk", x, "Compare objects a < b valueOf order");

x = "";
assertFalse(a < a, "Compare objects a < a");
// assertEquals("hesthest", x, "Compare objects a < a valueOf order");

x = "";
assertTrue(a == a, "Compare objects a == a");
assertEquals("", x, "Compare objects a == a valueOf not called");

x = "";
assertFalse(b > b, "Compare objects b > b");
assertEquals("fiskfisk", x, "Compare objects b > b valueOf order");

x = "";
assertTrue(b >= b, "Compare objects b >= b");
assertEquals("fiskfisk", x, "Compare objects b >= b valueOf order");

x = "";
assertFalse(a > b, "Compare objects a > b");
assertEquals("fiskhest", x, "Compare objects a > b valueOf order");

x = "";
assertFalse(a > void(0), "Compare objects a > undefined");
assertEquals("hest", x, "Compare objects a > undefined valueOf order");

x = "";
assertFalse(void(0) > b, "Compare objects undefined > b");
assertEquals("fisk", x, "Compare objects undefined > b valueOf order");


function identical_object_comparison() {
  x = "";
  assertTrue(a < b, "Compare objects a < b");
  assertEquals("hestfisk", x, "Compare objects a < b valueOf order");

  x = "";
  assertFalse(a < a, "Compare objects a < a");
  //  assertEquals("hesthest", x, "Compare objects a < a valueOf order");

  x = "";
  assertTrue(a == a, "Compare objects a == a");
  assertEquals("", x, "Compare objects a == a valueOf not called");

  x = "";
  assertFalse(b > b, "Compare objects b > b");
  assertEquals("fiskfisk", x, "Compare objects b > b valueOf order");

  x = "";
  assertTrue(b >= b, "Compare objects b >= b");
  assertEquals("fiskfisk", x, "Compare objects b >= b valueOf order");

  x = "";
  assertFalse(a > b, "Compare objects a > b");
  assertEquals("fiskhest", x, "Compare objects a > b valueOf order");

  x = "";
  assertFalse(a > void(0), "Compare objects a > undefined");
  assertEquals("hest", x, "Compare objects a > undefined valueOf order");

  x = "";
  assertFalse(void(0) > b, "Compare objects undefined > b");
  assertEquals("fisk", x, "Compare objects undefined > b valueOf order");
}

// Call inside loop to test optimization and possible caching.
for (i = 0; i < 3; ++i) {
  identical_object_comparison();
}


print("ok");
