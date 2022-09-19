// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let TestCoverage;
let TestCoverageNoGC;

let nop;
let gen;

!function() {
  function GetCoverage(source) {
    for (var script of %DebugCollectCoverage()) {
      if (script.script === source) return script;
    }
    return undefined;
  };

  function TestCoverageInternal(
      name, source, expectation, collect_garbage, prettyPrintResults) {
    source = source.trim();
    eval(source);
    if (collect_garbage) %CollectGarbage("collect dead objects");
    var covfefe = GetCoverage(source);
    var stringified_result = JSON.stringify(covfefe);
    var stringified_expectation = JSON.stringify(expectation);
    const mismatch = stringified_result != stringified_expectation;
    if (mismatch) {
      console.log(stringified_result.replace(/[}],[{]/g, "},\n {"));
    }
    if (prettyPrintResults) {
      console.log("=== Coverage Expectation ===")
      for (const {start,end,count} of expectation) {
        console.log(`Range [${start}, ${end}) (count: ${count})`);
        console.log(source.substring(start, end));
      }
      console.log("=== Coverage Results ===")
      for (const {start,end,count} of covfefe) {
        console.log(`Range [${start}, ${end}) (count: ${count})`);
        console.log(source.substring(start, end));
      }
      console.log("========================")
    }
    assertEquals(stringified_expectation, stringified_result, name + " failed");
  };

  TestCoverage = function(name, source, expectation, prettyPrintResults) {
    TestCoverageInternal(name, source, expectation, true, prettyPrintResults);
  };

  TestCoverageNoGC = function(name, source, expectation, prettyPrintResults) {
    TestCoverageInternal(name, source, expectation, false, prettyPrintResults);
  };

  nop = function() {};

  gen = function*() {
    yield 1;
    yield 2;
    yield 3;
  };
}();
