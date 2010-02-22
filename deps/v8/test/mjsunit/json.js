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
assertEquals("1970-01-01T00:00:00.000Z", new Date(0).toJSON());
assertEquals("1979-01-11T08:00:00.000Z", new Date("1979-01-11 08:00 GMT").toJSON());
assertEquals("2005-05-05T05:05:05.000Z", new Date("2005-05-05 05:05:05 GMT").toJSON());
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
assertEquals({42:37}, JSON.parse('{"42":37}'));
assertEquals(null, JSON.parse("null"));
assertEquals(true, JSON.parse("true"));
assertEquals(false, JSON.parse("false"));
assertEquals("foo", JSON.parse('"foo"'));
assertEquals("f\no", JSON.parse('"f\\no"'));
assertEquals("\b\f\n\r\t\"\u2028\/\\",
             JSON.parse('"\\b\\f\\n\\r\\t\\"\\u2028\\/\\\\"'));
assertEquals([1.1], JSON.parse("[1.1]"));
assertEquals([1], JSON.parse("[1.0]"));

assertEquals(0, JSON.parse("0"));
assertEquals(1, JSON.parse("1"));
assertEquals(0.1, JSON.parse("0.1"));
assertEquals(1.1, JSON.parse("1.1"));
assertEquals(1.1, JSON.parse("1.100000"));
assertEquals(1.111111, JSON.parse("1.111111"));
assertEquals(-0, JSON.parse("-0"));
assertEquals(-1, JSON.parse("-1"));
assertEquals(-0.1, JSON.parse("-0.1"));
assertEquals(-1.1, JSON.parse("-1.1"));
assertEquals(-1.1, JSON.parse("-1.100000"));
assertEquals(-1.111111, JSON.parse("-1.111111"));
assertEquals(11, JSON.parse("1.1e1"));
assertEquals(11, JSON.parse("1.1e+1"));
assertEquals(0.11, JSON.parse("1.1e-1"));
assertEquals(11, JSON.parse("1.1E1"));
assertEquals(11, JSON.parse("1.1E+1"));
assertEquals(0.11, JSON.parse("1.1E-1"));

assertEquals([], JSON.parse("[]"));
assertEquals([1], JSON.parse("[1]"));
assertEquals([1, "2", true, null], JSON.parse('[1, "2", true, null]'));

assertEquals("", JSON.parse('""'));
assertEquals(["", "", -0, ""], JSON.parse('[    ""  ,    ""  ,   -0,    ""]'));
assertEquals("", JSON.parse('""'));


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

// JavaScript number literals not valid in JSON.
TestInvalid('[01]');
TestInvalid('[.1]');
TestInvalid('[1.]');
TestInvalid('[1.e1]');
TestInvalid('[-.1]');
TestInvalid('[-1.]');

// Plain invalid number literals.
TestInvalid('-');
TestInvalid('--1');
TestInvalid('-1e');
TestInvalid('1e--1]');
TestInvalid('1e+-1');
TestInvalid('1e-+1');
TestInvalid('1e++1');

// JavaScript string literals not valid in JSON.
TestInvalid("'single quote'");  // Valid JavaScript
TestInvalid('"\\a invalid escape"');
TestInvalid('"\\v invalid escape"');  // Valid JavaScript
TestInvalid('"\\\' invalid escape"');  // Valid JavaScript
TestInvalid('"\\x42 invalid escape"');  // Valid JavaScript
TestInvalid('"\\u202 invalid escape"');
TestInvalid('"\\012 invalid escape"');
TestInvalid('"Unterminated string');
TestInvalid('"Unterminated string\\"');
TestInvalid('"Unterminated string\\\\\\"');

// JavaScript RegExp literals not valid in JSON.
TestInvalid('/true/');

// Test bad JSON that would be good JavaScript (ES5).
TestInvalid("{true:42}");
TestInvalid("{false:42}");
TestInvalid("{null:42}");
TestInvalid("{'foo':42}");
TestInvalid("{42:42}");
TestInvalid("{0:42}");
TestInvalid("{-1:42}");

// Test for trailing garbage detection.
TestInvalid('42 px');
TestInvalid('42 .2');
TestInvalid('42 2');
TestInvalid('42 e1');
TestInvalid('"42" ""');
TestInvalid('"42" ""');
TestInvalid('"" ""');
TestInvalid('true ""');
TestInvalid('false ""');
TestInvalid('null ""');
TestInvalid('null ""');
TestInvalid('[] ""');
TestInvalid('[true] ""');
TestInvalid('{} ""');
TestInvalid('{"x":true} ""');
TestInvalid('"Garbage""After string"');

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

// The gap is capped at ten characters if specified as string.
assertEquals('{\n          "a": "b",\n          "c": "d"\n}',
              JSON.stringify({a:"b",c:"d"}, null, 
                             "          /*characters after 10th*/"));

//The gap is capped at ten characters if specified as number.
assertEquals('{\n          "a": "b",\n          "c": "d"\n}',
              JSON.stringify({a:"b",c:"d"}, null, 15));

// Replaced wrapped primitives are unwrapped.
function newx(k, v)  { return (k == "x") ? new v(42) : v; }
assertEquals('{"x":"42"}', JSON.stringify({x: String}, newx));
assertEquals('{"x":42}', JSON.stringify({x: Number}, newx));
assertEquals('{"x":true}', JSON.stringify({x: Boolean}, newx));

assertEquals(undefined, JSON.stringify(undefined));
assertEquals(undefined, JSON.stringify(function () { }));
// Arrays with missing, undefined or function elements have those elements 
// replaced by null.
assertEquals("[null,null,null]", 
             JSON.stringify([undefined,,function(){}]));

// Objects with undefined or function properties (including replaced properties)
// have those properties ignored.
assertEquals('{}', 
             JSON.stringify({a: undefined, b: function(){}, c: 42, d: 42},
                            function(k, v) { if (k == "c") return undefined; 
                                             if (k == "d") return function(){};
                                             return v; }));

TestInvalid('1); throw "foo"; (1');

var x = 0;
eval("(1); x++; (1)");
TestInvalid('1); x++; (1');
