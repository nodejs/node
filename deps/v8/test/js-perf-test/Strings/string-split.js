// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ShortSubjectEmptySeparator', [5], [
  new Benchmark('ShortSubjectEmptySeparator', true, false, 0,
  ShortSubjectEmptySeparator),
]);

new BenchmarkSuite('LongSubjectEmptySeparator', [1000], [
  new Benchmark('LongSubjectEmptySeparator', true, false, 0,
  LongSubjectEmptySeparator),
]);

new BenchmarkSuite('ShortTwoBytesSubjectEmptySeparator', [5], [
  new Benchmark('ShortTwoBytesSubjectEmptySeparator', true, false, 0,
  ShortTwoBytesSubjectEmptySeparator),
]);

new BenchmarkSuite('LongTwoBytesSubjectEmptySeparator', [1000], [
  new Benchmark('LongTwoBytesSubjectEmptySeparator', true, false, 0,
  LongTwoBytesSubjectEmptySeparator),
]);

new BenchmarkSuite('ShortSubject', [5], [
  new Benchmark('ShortSubject', true, false, 0,
  ShortSubject),
]);

new BenchmarkSuite('LongSubject', [1000], [
  new Benchmark('LongSubject', true, false, 0,
  LongSubject),
]);

new BenchmarkSuite('ShortTwoBytesSubject', [5], [
  new Benchmark('ShortTwoBytesSubject', true, false, 0,
  ShortTwoBytesSubject),
]);

new BenchmarkSuite('LongTwoBytesSubject', [1000], [
  new Benchmark('LongTwoBytesSubject', true, false, 0,
  LongTwoBytesSubject),
]);

const shortString = "ababaabcdeaaaaaab";
const shortTwoBytesString = "\u0429\u0428\u0428\u0429\u0429\u0429\u0428\u0429\u0429";
// Use Array.join to create a flat string
const longString = new Array(0x500).fill("abcde").join('');
const longTwoBytesString = new Array(0x500).fill("\u0427\u0428\u0429\u0430").join('');

function ShortSubjectEmptySeparator() {
  shortString.split('');
}

function LongSubjectEmptySeparator() {
  longString.split('');
}

function ShortTwoBytesSubjectEmptySeparator() {
  shortTwoBytesString.split('');
}

function LongTwoBytesSubjectEmptySeparator() {
  longTwoBytesString.split('');
}

function ShortSubject() {
  shortString.split('a');
}

function LongSubject() {
  longString.split('a');
}

function ShortTwoBytesSubject() {
  shortTwoBytesString.split('\u0428');
}

function LongTwoBytesSubject() {
  longTwoBytesString.split('\u0428');
}
