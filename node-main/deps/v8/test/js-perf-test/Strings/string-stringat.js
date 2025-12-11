// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringCharCodeAtConstant', [3], [
  new Benchmark('StringCharCodeAtConstant', true, false, 0,
  StringCharCodeAtConstant),
]);

new BenchmarkSuite('StringCharCodeAtNonConstant', [3], [
  new Benchmark('StringCharCodeAtNonConstant', true, false, 0,
  StringCharCodeAtNonConstant),
]);

new BenchmarkSuite('StringCharCodeAtConstantInbounds', [3], [
  new Benchmark('StringCharCodeAtConstantInbounds', true, false, 0,
  StringCharCodeAtConstantInbounds),
]);

new BenchmarkSuite('StringCharCodeAtNonConstantInbounds', [3], [
  new Benchmark('StringCharCodeAtNonConstantInbounds', true, false, 0,
  StringCharCodeAtNonConstantInbounds),
]);

const string = "qweruiplkjhgfdsazxccvbnm";
const indices = [1, 13, 32, 100, "xx"];
const indicesInbounds = [1, 7, 13, 17, "xx"];

function StringCharCodeAtConstant() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += string.charCodeAt(indices[j] | 0);
  }

  return sum;
}

function StringCharCodeAtNonConstant() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += string.charCodeAt(indices[j]);
  }

  return sum;
}

function StringCharCodeAtConstantInbounds() {
  var sum = 0;

  for (var j = 0; j < indicesInbounds.length - 1; ++j) {
    sum += string.charCodeAt(indicesInbounds[j] | 0);
  }

  return sum;
}

function StringCharCodeAtNonConstantInbounds() {
  var sum = 0;

  for (var j = 0; j < indicesInbounds.length - 1; ++j) {
    sum += string.charCodeAt(indicesInbounds[j]);
  }

  return sum;
}

new BenchmarkSuite('StringCodePointAtConstant', [3], [
  new Benchmark('StringCodePointAtConstant', true, false, 0,
  StringCodePointAtConstant),
]);

new BenchmarkSuite('StringCodePointAtNonConstant', [3], [
  new Benchmark('StringCodePointAtNonConstant', true, false, 0,
  StringCodePointAtNonConstant),
]);

new BenchmarkSuite('StringCodePointAtConstantInbounds', [3], [
  new Benchmark('StringCodePointAtConstantInbounds', true, false, 0,
  StringCodePointAtConstantInbounds),
]);

new BenchmarkSuite('StringCodePointAtNonConstantInbounds', [3], [
  new Benchmark('StringCodePointAtNonConstantInbounds', true, false, 0,
  StringCodePointAtNonConstantInbounds),
]);

const unicode_string = "qweräϠ�𝌆krefdäϠ�𝌆ccäϠ�𝌆";

function StringCodePointAtConstant() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += unicode_string.codePointAt(indices[j] | 0);
  }

  return sum;
}

function StringCodePointAtNonConstant() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += unicode_string.codePointAt(indices[j]);
  }

  return sum;
}

function StringCodePointAtConstantInbounds() {
  var sum = 0;

  for (var j = 0; j < indicesInbounds.length - 1; ++j) {
    sum += unicode_string.codePointAt(indicesInbounds[j] | 0);
  }

  return sum;
}

function StringCodePointAtNonConstantInbounds() {
  var sum = 0;

  for (var j = 0; j < indicesInbounds.length - 1; ++j) {
    sum += unicode_string.codePointAt(indicesInbounds[j]);
  }

  return sum;
}
