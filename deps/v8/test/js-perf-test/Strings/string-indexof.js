// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringIndexOfConstant', [5], [
  new Benchmark('StringIndexOfConstant', true, false, 0,
  StringIndexOfConstant),
]);

new BenchmarkSuite('StringIndexOfNonConstant', [5], [
  new Benchmark('StringIndexOfNonConstant', true, false, 0,
  StringIndexOfNonConstant),
]);

const subject = "aaaaaaaaaaaaaaaab";
const searches = ['a', 'b', 'c'];

function StringIndexOfConstant() {
  var sum = 0;

  for (var j = 0; j < searches.length; ++j) {
    sum += subject.indexOf("" + searches[j]);
  }

  return sum;
}

function StringIndexOfNonConstant() {
  var sum = 0;

  for (var j = 0; j < searches.length; ++j) {
    sum += subject.indexOf(searches[j]);
  }

  return sum;
}

new BenchmarkSuite('StringCharCodeAtConstantWithOutOfBounds', [3], [
  new Benchmark('StringCharCodeAtConstantWithOutOfBounds', true, false, 0,
  StringCharCodeAtConstantWithOutOfBounds),
]);

new BenchmarkSuite('StringCharCodeAtNonConstantWithOutOfBounds', [3], [
  new Benchmark('StringCharCodeAtNonConstantWithOutOfBounds', true, false, 0,
  StringCharCodeAtNonConstantWithOutOfBounds),
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

function StringCharCodeAtConstantWithOutOfBounds() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += string.charCodeAt(indices[j] | 0);
  }

  return sum;
}

function StringCharCodeAtNonConstantWithOutOfBounds() {
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

new BenchmarkSuite('StringCodePointAtConstantWithOutOfBounds', [3], [
  new Benchmark('StringCodePointAtConstantWithOutOfBounds', true, false, 0,
  StringCodePointAtConstantWithOutOfBounds),
]);

new BenchmarkSuite('StringCodePointAtNonConstantWithOutOfBounds', [3], [
  new Benchmark('StringCodePointAtNonConstantWithOutOfBounds', true, false, 0,
  StringCodePointAtNonConstantWithOutOfBounds),
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

function StringCodePointAtConstantWithOutOfBounds() {
  var sum = 0;

  for (var j = 0; j < indices.length - 1; ++j) {
    sum += unicode_string.codePointAt(indices[j] | 0);
  }

  return sum;
}

function StringCodePointAtNonConstantWithOutOfBounds() {
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
