// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assert(expression, message) {
  if (typeof expression === "string" && message === void 0) {
    message = expression;
    expression = eval(expression);
  }

  if (!expression) {
    var lines = ["Benchmark Error"];
    if (message !== void 0) {
      lines = ["Benchmark Error:", String(message)];
    }
    throw new Error(lines.join("\n"));
  }
  return true;
}

assert.same = function(expected, actual, message) {
  var isSame =
      expected === actual || typeof expected !== expected && actual !== actual;
  if (!isSame) {
    var details = `Expected:  ${String(expected)}\n` +
                  `But found: ${String(actual)}`;
    var lines = ["Benchmark Error:", details];
    if (message !== void 0) {
      lines = ["Benchmark Error:", details, "", String(message)];
    }
    throw new Error(lines.join("\n"));
  }
  return true;
}

new BenchmarkSuite('Spread_OneByteShort', [1000], [
  new Benchmark('test', false, false, 0,
                Spread_OneByteShort, Spread_OneByteShortSetup,
                Spread_OneByteShortTearDown),
]);

var result;
var string;
function Spread_OneByteShortSetup() {
  result = undefined;
  string = "Alphabet-Soup";
}

function Spread_OneByteShort() {
  result = [...string];
}

function Spread_OneByteShortTearDown() {
  var expected = "A|l|p|h|a|b|e|t|-|S|o|u|p";
  return assert("Array.isArray(result)")
      && assert.same(expected, result.join("|"));
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('Spread_TwoByteShort', [1000], [
  new Benchmark('test', false, false, 0,
                Spread_TwoByteShort, Spread_TwoByteShortSetup,
                Spread_TwoByteShortTearDown),
]);

function Spread_TwoByteShortSetup() {
  result = undefined;
  string = "\u5FCD\u8005\u306E\u653B\u6483";
}

function Spread_TwoByteShort() {
  result = [...string];
}

function Spread_TwoByteShortTearDown() {
  var expected = "\u5FCD|\u8005|\u306E|\u653B|\u6483";
  return assert("Array.isArray(result)")
      && assert.same(expected, result.join("|"));
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('Spread_WithSurrogatePairsShort', [1000], [
  new Benchmark('test', false, false, 0,
                Spread_WithSurrogatePairsShort,
                Spread_WithSurrogatePairsShortSetup,
                Spread_WithSurrogatePairsShortTearDown),
]);

function Spread_WithSurrogatePairsShortSetup() {
  result = undefined;
  string = "\uD83C\uDF1F\u5FCD\u8005\u306E\u653B\u6483\uD83C\uDF1F";
}

function Spread_WithSurrogatePairsShort() {
  result = [...string];
}

function Spread_WithSurrogatePairsShortTearDown() {
  var expected =
      "\uD83C\uDF1F|\u5FCD|\u8005|\u306E|\u653B|\u6483|\uD83C\uDF1F";
  return assert("Array.isArray(result)")
      && assert.same(expected, result.join("|"));
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_OneByteShort', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_OneByteShort, ForOf_OneByteShortSetup,
                ForOf_OneByteShortTearDown),
]);

function ForOf_OneByteShortSetup() {
  result = undefined;
  string = "Alphabet-Soup";
}

function ForOf_OneByteShort() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_OneByteShortTearDown() {
  return assert.same(string, result);
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_TwoByteShort', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_TwoByteShort, ForOf_TwoByteShortSetup,
                ForOf_TwoByteShortTearDown),
]);

function ForOf_TwoByteShortSetup() {
  result = undefined;
  string = "\u5FCD\u8005\u306E\u653B\u6483";
}

function ForOf_TwoByteShort() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_TwoByteShortTearDown() {
  return assert.same(string, result);
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_WithSurrogatePairsShort', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_WithSurrogatePairsShort,
                ForOf_WithSurrogatePairsShortSetup,
                ForOf_WithSurrogatePairsShortTearDown),
]);

function ForOf_WithSurrogatePairsShortSetup() {
  result = undefined;
  string = "\uD83C\uDF1F\u5FCD\u8005\u306E\u653B\u6483\uD83C\uDF1F";
}

function ForOf_WithSurrogatePairsShort() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_WithSurrogatePairsShortTearDown() {
  return assert.same(string, result);
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_OneByteLong', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_OneByteLong, ForOf_OneByteLongSetup,
                ForOf_OneByteLongTearDown),
]);

function ForOf_OneByteLongSetup() {
  result = undefined;
  string = "Alphabet-Soup|".repeat(128);
}

function ForOf_OneByteLong() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_OneByteLongTearDown() {
  return assert.same(string, result);
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_TwoByteLong', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_OneByteLong, ForOf_OneByteLongSetup,
                ForOf_OneByteLongTearDown),
]);

function ForOf_TwoByteLongSetup() {
  result = undefined;
  string = "\u5FCD\u8005\u306E\u653B\u6483".repeat(128);
}

function ForOf_TwoByteLong() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_TwoByteLongTearDown() {
  return assert.same(string, result);
}

// ----------------------------------------------------------------------------

new BenchmarkSuite('ForOf_WithSurrogatePairsLong', [1000], [
  new Benchmark('test', false, false, 0,
                ForOf_WithSurrogatePairsLong, ForOf_WithSurrogatePairsLongSetup,
                ForOf_WithSurrogatePairsLongTearDown),
]);

function ForOf_WithSurrogatePairsLongSetup() {
  result = undefined;
  string = "\uD83C\uDF1F\u5FCD\u8005\u306E\u653B\u6483\uD83C\uDF1F|"
      .repeat(128);
}

function ForOf_WithSurrogatePairsLong() {
  result = "";
  for (var c of string) result += c;
}

function ForOf_WithSurrogatePairsLongTearDown() {
  return assert.same(string, result);
}
