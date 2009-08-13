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

function GenericToJSONChecks(Constructor, value, alternative) {
  var n1 = new Constructor(value);
  n1.valueOf = function () { return alternative; };
  assertEquals(alternative, n1.toJSON());
  var n2 = new Constructor(value);
  n2.valueOf = null;
  assertThrows(function () { n2.toJSON(); }, TypeError);
  var n3 = new Constructor(value);
  n3.valueOf = function () { return {}; };
  assertThrows(function () { n3.toJSON(); }, TypeError, 'result_not_primitive');
  var n4 = new Constructor(value);
  n4.valueOf = function () {
    assertEquals(0, arguments.length);
    assertEquals(this, n4);
    return null;
  };
  assertEquals(null, n4.toJSON());
}

// Number toJSON
assertEquals(3, (3).toJSON());
assertEquals(3, (3).toJSON(true));
assertEquals(4, (new Number(4)).toJSON());
GenericToJSONChecks(Number, 5, 6);

// Boolean toJSON
assertEquals(true, (true).toJSON());
assertEquals(true, (true).toJSON(false));
assertEquals(false, (false).toJSON());
assertEquals(true, (new Boolean(true)).toJSON());
GenericToJSONChecks(Boolean, true, false);
GenericToJSONChecks(Boolean, false, true);

// String toJSON
assertEquals("flot", "flot".toJSON());
assertEquals("flot", "flot".toJSON(3));
assertEquals("tolf", (new String("tolf")).toJSON());
GenericToJSONChecks(String, "x", "y");

// Date toJSON
assertEquals("1970-01-01T00:00:00Z", new Date(0).toJSON());
assertEquals("1979-01-11T08:00:00Z", new Date("1979-01-11 08:00 GMT").toJSON());
assertEquals("2005-05-05T05:05:05Z", new Date("2005-05-05 05:05:05 GMT").toJSON());
var n1 = new Date(10000);
n1.toISOString = function () { return "foo"; };
assertEquals("foo", n1.toJSON());
var n2 = new Date(10001);
n2.toISOString = null;
assertThrows(function () { n2.toJSON(); }, TypeError);
var n3 = new Date(10002);
n3.toISOString = function () { return {}; };
assertThrows(function () { n3.toJSON(); }, TypeError, "result_not_primitive");
var n4 = new Date(10003);
n4.toISOString = function () {
  assertEquals(0, arguments.length);
  assertEquals(this, n4);
  return null;
};
assertEquals(null, n4.toJSON());

assertEquals(Object.prototype, JSON.__proto__);
assertEquals("[object JSON]", Object.prototype.toString.call(JSON));

// DontEnum
for (var p in this)
  assertFalse(p == "JSON");

// Parse

assertEquals({}, JSON.parse("{}"));
assertEquals(null, JSON.parse("null"));
assertEquals(true, JSON.parse("true"));
assertEquals(false, JSON.parse("false"));
assertEquals("foo", JSON.parse('"foo"'));
assertEquals("f\no", JSON.parse('"f\\no"'));
assertEquals(1.1, JSON.parse("1.1"));
assertEquals(1, JSON.parse("1.0"));
assertEquals(0.0000000003, JSON.parse("3e-10"));
assertEquals([], JSON.parse("[]"));
assertEquals([1], JSON.parse("[1]"));
assertEquals([1, "2", true, null], JSON.parse('[1, "2", true, null]'));

function GetFilter(name) {
  function Filter(key, value) {
    return (key == name) ? undefined : value;
  }
  return Filter;
}

var pointJson = '{"x": 1, "y": 2}';
assertEquals({'x': 1, 'y': 2}, JSON.parse(pointJson));
assertEquals({'x': 1}, JSON.parse(pointJson, GetFilter('y')));
assertEquals({'y': 2}, JSON.parse(pointJson, GetFilter('x')));
assertEquals([1, 2, 3], JSON.parse("[1, 2, 3]"));
assertEquals([1, undefined, 3], JSON.parse("[1, 2, 3]", GetFilter(1)));
assertEquals([1, 2, undefined], JSON.parse("[1, 2, 3]", GetFilter(2)));

function DoubleNumbers(key, value) {
  return (typeof value == 'number') ? 2 * value : value;
}

var deepObject = '{"a": {"b": 1, "c": 2}, "d": {"e": {"f": 3}}}';
assertEquals({"a": {"b": 1, "c": 2}, "d": {"e": {"f": 3}}},
             JSON.parse(deepObject));
assertEquals({"a": {"b": 2, "c": 4}, "d": {"e": {"f": 6}}},
             JSON.parse(deepObject, DoubleNumbers));

function TestInvalid(str) {
  assertThrows(function () { JSON.parse(str); }, SyntaxError);
}

TestInvalid('abcdef');
TestInvalid('isNaN()');
TestInvalid('{"x": [1, 2, deepObject]}');
TestInvalid('[1, [2, [deepObject], 3], 4]');
TestInvalid('function () { return 0; }');

TestInvalid("[1, 2");
TestInvalid('{"x": 3');

// Stringify

assertEquals("true", JSON.stringify(true));
assertEquals("false", JSON.stringify(false));
assertEquals("null", JSON.stringify(null));
assertEquals("false", JSON.stringify({toJSON: function () { return false; }}));
assertEquals("4", JSON.stringify(4));
assertEquals('"foo"', JSON.stringify("foo"));
assertEquals("null", JSON.stringify(Infinity));
assertEquals("null", JSON.stringify(-Infinity));
assertEquals("null", JSON.stringify(NaN));
assertEquals("4", JSON.stringify(new Number(4)));
assertEquals('"bar"', JSON.stringify(new String("bar")));

assertEquals('"foo\\u0000bar"', JSON.stringify("foo\0bar"));
assertEquals('"f\\"o\'o\\\\b\\ba\\fr\\nb\\ra\\tz"',
             JSON.stringify("f\"o\'o\\b\ba\fr\nb\ra\tz"));

assertEquals("[1,2,3]", JSON.stringify([1, 2, 3]));
assertEquals("[\n 1,\n 2,\n 3\n]", JSON.stringify([1, 2, 3], null, 1));
assertEquals("[\n  1,\n  2,\n  3\n]", JSON.stringify([1, 2, 3], null, 2));
assertEquals("[\n  1,\n  2,\n  3\n]",
             JSON.stringify([1, 2, 3], null, new Number(2)));
assertEquals("[\n^1,\n^2,\n^3\n]", JSON.stringify([1, 2, 3], null, "^"));
assertEquals("[\n^1,\n^2,\n^3\n]",
             JSON.stringify([1, 2, 3], null, new String("^")));
assertEquals("[\n 1,\n 2,\n [\n  3,\n  [\n   4\n  ],\n  5\n ],\n 6,\n 7\n]",
             JSON.stringify([1, 2, [3, [4], 5], 6, 7], null, 1));
assertEquals("[]", JSON.stringify([], null, 1));
assertEquals("[1,2,[3,[4],5],6,7]",
             JSON.stringify([1, 2, [3, [4], 5], 6, 7], null));
assertEquals("[2,4,[6,[8],10],12,14]",
             JSON.stringify([1, 2, [3, [4], 5], 6, 7], DoubleNumbers));

var circular = [1, 2, 3];
circular[2] = circular;
assertThrows(function () { JSON.stringify(circular); }, TypeError);

var singleton = [];
var multiOccurrence = [singleton, singleton, singleton];
assertEquals("[[],[],[]]", JSON.stringify(multiOccurrence));

assertEquals('{"x":5,"y":6}', JSON.stringify({x:5,y:6}));
assertEquals('{"x":5}', JSON.stringify({x:5,y:6}, ['x']));
assertEquals('{\n "a": "b",\n "c": "d"\n}',
             JSON.stringify({a:"b",c:"d"}, null, 1));
assertEquals('{"y":6,"x":5}', JSON.stringify({x:5,y:6}, ['y', 'x']));

assertEquals(undefined, JSON.stringify(undefined));
assertEquals(undefined, JSON.stringify(function () { }));

function checkIllegal(str) {
  assertThrows(function () { JSON.parse(str); }, SyntaxError);
}

checkIllegal('1); throw "foo"; (1');

var x = 0;
eval("(1); x++; (1)");
checkIllegal('1); x++; (1');
