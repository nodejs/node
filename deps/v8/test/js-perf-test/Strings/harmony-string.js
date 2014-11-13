// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringFunctions', [1000], [
  new Benchmark('StringRepeat', false, false, 0,
                Repeat, RepeatSetup, RepeatTearDown),
  new Benchmark('StringStartsWith', false, false, 0,
                StartsWith, WithSetup, WithTearDown),
  new Benchmark('StringEndsWith', false, false, 0,
                EndsWith, WithSetup, WithTearDown),
  new Benchmark('StringContains', false, false, 0,
                Contains, ContainsSetup, WithTearDown),
  new Benchmark('StringFromCodePoint', false, false, 0,
                FromCodePoint, FromCodePointSetup, FromCodePointTearDown),
  new Benchmark('StringCodePointAt', false, false, 0,
                CodePointAt, CodePointAtSetup, CodePointAtTearDown),
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
  for(var i = 0; i < 1000; i++) {
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

function ContainsSetup() {
  str = "def".repeat(100) + "abc".repeat(100) + "qqq".repeat(100);
  substr = "abc".repeat(100);
}

function Contains() {
  result = str.contains(substr);
}

var MAX_CODE_POINT = 0xFFFFF;

function FromCodePointSetup() {
  result = new Array(MAX_CODE_POINT + 1);
}

function FromCodePoint() {
  for (var i = 0; i <= MAX_CODE_POINT; i++) {
    result[i] = String.fromCodePoint(i);
  }
}

function FromCodePointTearDown() {
  for (var i = 0; i <= MAX_CODE_POINT; i++) {
    if (i !== result[i].codePointAt(0)) return false;
  }
  return true;
}


var allCodePoints;

function CodePointAtSetup() {
  allCodePoints = new Array(MAX_CODE_POINT + 1);
  for (var i = 0; i <= MAX_CODE_POINT; i++) {
    allCodePoints = String.fromCodePoint(i);
  }
  result = undefined;
}

function CodePointAt() {
  result = 0;
  for (var i = 0; i <= MAX_CODE_POINT; i++) {
    result += allCodePoints.codePointAt(i);
  }
}

function CodePointAtTearDown() {
  return result === MAX_CODE_POINT * (MAX_CODE_POINT + 1) / 2;
}
