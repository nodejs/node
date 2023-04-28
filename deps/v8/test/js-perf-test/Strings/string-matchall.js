// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {

function Internalize(s) {
  return Object.keys({[s]:0})[0];
}

const string = Internalize('a'.repeat(100));
let pattern;
let result;

const setupString = () => pattern = '.';
const setupRegExp = () => pattern = /./g;
const setupZeroWidth = () => pattern = /(?:)/g;
const setupZeroWidthUnicode = () => pattern = /(?:)/gu;

function benchIteratorCreation() {
  result = string.matchAll(pattern);
}

function benchBuiltin() {
  for (const match of string.matchAll(pattern)) {
    result = match[0];
  }
}

function benchManualString() {
  let regexp = new RegExp(pattern, 'g');
  let match;
  while ((match = regexp.exec(string)) !== null) {
    result = match[0];
  }
}

function benchManualRegExp() {
  let match;
  while ((match = pattern.exec(string)) !== null) {
    result = match[0];
  }
}

// Iterator Creation
new BenchmarkSuite('StringMatchAllBuiltinRegExpIteratorCreation', [20], [
  new Benchmark('StringMatchAllBuiltinRegExpIteratorCreation', false, false, 0, benchIteratorCreation, setupRegExp)
]);
new BenchmarkSuite('StringMatchAllBuiltinStringIteratorCreation', [20], [
  new Benchmark('StringMatchAllBuiltinStringIteratorCreation', false, false, 0, benchIteratorCreation, setupString)
]);

// String
new BenchmarkSuite('StringMatchAllBuiltinString', [20], [
  new Benchmark('StringMatchAllBuiltinString', false, false, 0, benchBuiltin, setupString)
]);
new BenchmarkSuite('StringMatchAllManualString', [20], [
  new Benchmark('StringMatchAllManualString', false, false, 0, benchManualString, setupString)
]);

// RegExp
new BenchmarkSuite('StringMatchAllBuiltinRegExp', [20], [
  new Benchmark('StringMatchAllBuiltinRegExp', false, false, 0, benchBuiltin, setupRegExp)
]);
new BenchmarkSuite('StringMatchAllManualRegExp', [20], [
  new Benchmark('StringMatchAllManualRegExp', false, false, 0, benchManualRegExp, setupRegExp)
]);

// Zero width
new BenchmarkSuite('StringMatchAllBuiltinZeroWidth', [20], [
  new Benchmark('StringMatchAllBuiltinZeroWidth', false, false, 0, benchBuiltin, setupZeroWidth)
]);
new BenchmarkSuite('StringMatchAllBuiltinZeroWidthUnicode', [20], [
  new Benchmark('StringMatchAllBuiltinZeroWidthUnicode', false, false, 0, benchBuiltin, setupZeroWidthUnicode)
]);

})();
