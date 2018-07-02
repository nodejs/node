// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createSuiteWithWarmup( name, count, fn) {
  new BenchmarkSuite(name, [count], [
    new Benchmark(name, true, false, 0, fn),
  ]);
}

const inputs = [
  'I\xF1t\xEBrn\xE2ti\xF4n\xE0liz\xE6ti\xF8n\u2603\uD83D\uDCA9\uFFFD',
  'Lorem ipsum dolor sit amet, consectetur adipiscing elit.',
  'Integer eu augue suscipit, accumsan ipsum nec, sagittis sem.',
  'In vitae pellentesque dolor. Curabitur leo nunc, luctus vitae',
  'risus eget, fermentum hendrerit justo.',
];
const first = 'I';

function helper(fn) {
  let sum = 0;
  for (const input of inputs) {
    sum += fn(input);
  }
  return sum;
}

function startsWith(string) {
  return string.startsWith(first);
}

function startsIndex(string) {
  return string[0] === first;
}

function endsWith(string) {
  return string.endsWith(first);
}

function endsIndex(string) {
  return string[string.length - 1] === first;
}

createSuiteWithWarmup('startsWith', 1, () => helper(startsWith));
createSuiteWithWarmup('startsIndex', 1, () => helper(startsIndex));
createSuiteWithWarmup('endsWith', 1, () => helper(endsWith));
createSuiteWithWarmup('endsIndex', 1, () => helper(endsIndex));
