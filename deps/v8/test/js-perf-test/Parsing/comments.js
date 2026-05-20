// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite("OneLineComment", [1000], [
  new Benchmark("OneLineComment", false, true, iterations, Run, OneLineCommentSetup)
]);

new BenchmarkSuite("OneLineComments", [1000], [
  new Benchmark("OneLineComments", false, true, iterations, Run, OneLineCommentsSetup)
]);

new BenchmarkSuite("MultiLineComment", [1000], [
  new Benchmark("MultiLineComment", false, true, iterations, Run, MultiLineCommentSetup)
]);

function OneLineCommentSetup() {
  code = "//" + " This is a comment... ".repeat(600);
  %FlattenString(code);
}

function OneLineCommentsSetup() {
  code = "// This is a comment.\n".repeat(600);
  %FlattenString(code);
}

function MultiLineCommentSetup() {
  code = "/*" + " This is a comment... ".repeat(600) + "*/";
  %FlattenString(code);
}

function Run() {
  if (code == undefined) {
    throw new Error("No test data");
  }
  eval(code);
}
