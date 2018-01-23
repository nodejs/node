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

new BenchmarkSuite('StringCharCodeAtConstant', [3], [
  new Benchmark('StringIndexOfConstant', true, false, 0,
  StringCharCodeAtConstant),
]);

new BenchmarkSuite('StringCharCodeAtNonConstant', [3], [
  new Benchmark('StringIndexOfNonConstant', true, false, 0,
  StringCharCodeAtNonConstant),
]);

const string = "qweruiplkjhgfdsazxccvbnm";
const indices = [1, 13, 32, 100, "xx"];

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
