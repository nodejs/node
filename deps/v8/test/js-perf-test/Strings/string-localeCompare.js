// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringLocaleCompare', [1000000], [
  new Benchmark('StringLocaleCompare', false, false, 0,
  StringLocaleCompare),
]);

function StringLocaleCompare() {
  var array = ["XYZ", "mno", "abc", "EFG", "ijk", "123", "tuv", "234", "efg"];

  var sum = 0;
  for (var j = 0; j < array.length; ++j) {
    sum += "fox".localeCompare(array[j]);
  }

  return sum;
}
