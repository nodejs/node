// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that protocol works with different locales');

Protocol.Runtime.enable();

Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);

InspectorTest.runTestSuite([
  function consoleLogWithDefaultLocale(next) {
    Protocol.Runtime.evaluate({ expression: "console.log(239) "}).then(next);
  },

  function consoleTimeWithCommaAsSeparator(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    utils.setlocale("fr_CA.UTF-8");
    utils.setCurrentTimeMSForTest(0.0);
    Protocol.Runtime.evaluate({ expression: "console.time(\"a\");"})
      .then(() => utils.setCurrentTimeMSForTest(0.001))
      .then(() => Protocol.Runtime.evaluate({ expression: "console.timeEnd(\"a\");"}))
      .then(next);
  },

  function consoleLogWithCommaAsSeparator(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    utils.setlocale("fr_CA.UTF-8");
    Protocol.Runtime.evaluate({ expression: "console.log(239) "}).then(next);
  },

  function consoleTimeWithCommaAfterConsoleLog(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    utils.setlocale("fr_CA.UTF-8");
    Protocol.Runtime.evaluate({ expression: "console.log(239) "})
      .then(() => utils.setCurrentTimeMSForTest(0.0))
      .then(() => Protocol.Runtime.evaluate({ expression: "console.time(\"a\");"}))
      .then(() => utils.setCurrentTimeMSForTest(0.001))
      .then(() => Protocol.Runtime.evaluate({ expression: "console.timeEnd(\"a\");"}))
      .then(next);
  }
]);
