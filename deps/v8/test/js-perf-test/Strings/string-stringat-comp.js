// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

createSuiteWithWarmup('charCodeAt_const', 1, () => helper(charCodeAt));
createSuiteWithWarmup('charCodeAt_both', 1, () => helper(charCodeAtBoth));
createSuiteWithWarmup('charAt_const', 1, () => helper(charAt));
createSuiteWithWarmup('charAt_never', 1, () => helper(charAtNever));
createSuiteWithWarmup('charAt_both', 1, () => helper(charAtBoth));
createSuiteWithWarmup('stringIndex_const', 1, () => helper(stringIndex));
