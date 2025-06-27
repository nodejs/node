// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringRepeat', [10], [
  new Benchmark('StringRepeat', false, false, 0,
    Repeat, RepeatSetup, RepeatTearDown),
]);

new BenchmarkSuite('StringStartsWith', [10], [
  new Benchmark('StringStartsWith', false, false, 0,
    StartsWith, WithSetup, WithTearDown),
]);

new BenchmarkSuite('StringEndsWith', [10], [
  new Benchmark('StringEndsWith', false, false, 0,
    EndsWith, WithSetup, WithTearDown),
]);

new BenchmarkSuite('StringIncludes', [10], [
  new Benchmark('StringIncludes', false, false, 0,
    Includes, IncludesSetup, WithTearDown),
]);

new BenchmarkSuite('StringFromCodePoint', [10000], [
  new Benchmark('StringFromCodePoint', false, false, 0,
    FromCodePoint, FromCodePointSetup, FromCodePointTearDown),
]);

new BenchmarkSuite('StringCodePointAt', [1000], [
  new Benchmark('StringCodePointAt', false, false, 0,
    CodePointAt, CodePointAtSetup, CodePointAtTearDown),
]);

new BenchmarkSuite('StringCodePointAtSum', [100000], [
  new Benchmark('StringCodePointAtSum', false, true, 3,
    CodePointAtSum, CodePointAtSumSetup),
]);


var result;

var stringRepeatSource = "abc";

function RepeatSetup() {
  result = undefined;
}

function Repeat() {
  result = stringRepeatSource.repeat(500);
}

function RepeatTearDown() {
  var expected = "";
  for (var i = 0; i < 1000; i++) {
    expected += stringRepeatSource;
  }
  return result === expected;
}


var str;
var substr;

function WithSetup() {
  str = "abc".repeat(500);
  substr = "abc".repeat(200);
  result = undefined;
}

function WithTearDown() {
  return !!result;
}

function StartsWith() {
  result = str.startsWith(substr);
}

function EndsWith() {
  result = str.endsWith(substr);
}

function IncludesSetup() {
  str = "def".repeat(100) + "abc".repeat(100) + "qqq".repeat(100);
  substr = "abc".repeat(100);
}

function Includes() {
  result = str.includes(substr);
}

var MAX_CODE_POINT = 0xFFFFF;
const K = 1024;

function FromCodePointSetup() {
  result = new Array((MAX_CODE_POINT + 1) / K);
}

function FromCodePoint() {
  for (var i = 0; i <= MAX_CODE_POINT; i += K) {
    result[i] = String.fromCodePoint(i);
  }
}

function FromCodePointTearDown() {
  for (var i = 0; i <= MAX_CODE_POINT; i += K) {
    if (i !== result[i].codePointAt(0)) return false;
  }
  return true;
}


var allCodePoints;

function CodePointAtSetup() {
  allCodePoints = new Array((MAX_CODE_POINT + 1) / K);
  for (var i = 0; i <= MAX_CODE_POINT; i += K) {
    allCodePoints = String.fromCodePoint(i);
  }
  result = undefined;
}

function CodePointAt() {
  result = 0;
  for (var i = 0; i <= MAX_CODE_POINT; i += K) {
    result += allCodePoints.codePointAt(i);
  }
}

function CodePointAtTearDown() {
  return result === (MAX_CODE_POINT / K) * ((MAX_CODE_POINT / K) + 1) / 2;
}

var payload;

function CodePointAtSumSetup() {
  payload = "abcdefghijklmnopqrstuvwxyz";
  for(var j = 0; j < 16; ++j) payload += payload;
}

function CodePointAtSum() {
  var c = 0;
  for(j=payload.length-1; j >=0; --j) c+=payload.charCodeAt(j);
  return c;
}
