// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(f, name) {
  new BenchmarkSuite(name, [1], [
    new Benchmark(name, true, false, 0, f),
  ]);
}

const input = 'äϠ�𝌆 Lorem ipsum test test';

function helper(fn) {
  var sum = 0;
  for (var i = 0; i < input.length; i++) {
    sum += fn(input, i, i);
  }
  return sum;
}

function charCodeAt(str, i) {
  return str.charCodeAt(i) === 116;
}

function charCodeAtBoth(str, i, j) {
  return str.charCodeAt(j) == str.charCodeAt(i);
}

function charAt(str, i) {
  return 't' == str.charAt(i);
}

function charAtNever(str, i) {
  return '𝌆' == str.charAt(i);
}

function charAtBoth(str, i, j) {
  return str.charAt(j) == str.charAt(i);
}

function stringIndex(str, i) {
  return str[i] === 't';
}

benchy(() => helper(charCodeAt), "charCodeAt_const");
benchy(() => helper(charCodeAtBoth), "charCodeAt_both");
benchy(() => helper(charAt), "charAt_const");
benchy(() => helper(charAtNever), "charAt_never");
benchy(() => helper(charAtBoth), "charAt_both");
benchy(() => helper(stringIndex), "stringIndex_const");
