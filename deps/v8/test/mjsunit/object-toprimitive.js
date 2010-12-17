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

// Test the ToPrimitive internal function used by ToNumber/ToString.
// Does it [[Get]] and [[Call]] the object's toString and valueOf properties
// correctly. Specifically, does it call [[Get]] only once per property.

var o1 = { toString: function() { return 42; },
           valueOf: function() { return "37"; } };
var n1 = Number(o1);
var s1 = String(o1);
assertTrue(typeof n1 == "number");
assertTrue(typeof s1 == "string");

var trace = [];
var valueOfValue = 42;
var toStringValue = "foo";
function traceValueOf () {
  trace.push("vo");
  return valueOfValue;
};
function traceToString() {
  trace.push("ts");
  return toStringValue;
};
var valueOfFunc = traceValueOf;
var toStringFunc = traceToString;

var ot = { get toString() { trace.push("gts");
                            return toStringFunc; },
           get valueOf() { trace.push("gvo");
                           return valueOfFunc; }
};

var nt = Number(ot);
assertEquals(42, nt);
assertEquals(["gvo","vo"], trace);

trace = [];
var st = String(ot);
assertEquals("foo", st);
assertEquals(["gts","ts"], trace);

trace = [];
valueOfValue = ["not primitive"];
var nt = Number(ot);
assertEquals(Number("foo"), nt);
assertEquals(["gvo", "vo", "gts", "ts"], trace);

trace = [];
valueOfValue = 42;
toStringValue = ["not primitive"];
var st = String(ot);
assertEquals(String(42), st);
assertEquals(["gts", "ts", "gvo", "vo"], trace);

trace = [];
valueOfValue = ["not primitive"];
assertThrows("Number(ot)", TypeError);
assertEquals(["gvo", "vo", "gts", "ts"], trace);


toStringFunc = "not callable";
trace = [];
valueOfValue = 42;
var st = String(ot);
assertEquals(String(42), st);
assertEquals(["gts", "gvo", "vo"], trace);

valueOfFunc = "not callable";
trace = [];
assertThrows("String(ot)", TypeError);
assertEquals(["gts", "gvo"], trace);

toStringFunc = traceToString;
toStringValue = "87";
trace = [];
var nt = Number(ot);
assertEquals(87, nt);
assertEquals(["gvo", "gts", "ts"], trace);
