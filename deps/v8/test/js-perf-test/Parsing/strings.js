// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite("SingleLineString", [1000], [
  new Benchmark("SingleLineString", false, true, iterations, Run, SingleLineStringSetup)
]);

new BenchmarkSuite("SingleLineStrings", [3000], [
  new Benchmark("SingleLineStrings", false, true, iterations, Run, SingleLineStringsSetup)
]);

new BenchmarkSuite("MultiLineString", [1000], [
  new Benchmark("MultiLineString", false, true, iterations, Run, MultiLineStringSetup)
]);

function SingleLineStringSetup() {
  code = "\"" + "This is a string".repeat(600) + "\"";
  %FlattenString(code);
}

function SingleLineStringsSetup() {
  code = "\"This is a string\"\n".repeat(600);
  %FlattenString(code);
}

function MultiLineStringSetup() {
  code = "\"" + "This is a string \\\n".repeat(600) + "\"";
  %FlattenString(code);
}

function Run() {
  if (code == undefined) {
    throw new Error("No test data");
  }
  eval(code);
}
