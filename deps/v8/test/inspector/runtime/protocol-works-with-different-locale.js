// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Protocol.Runtime.enable();

Protocol.Runtime.onConsoleAPICalled(dumpConsoleApiCalled);

InspectorTest.runTestSuite([
  function consoleLogWithDefaultLocale(next) {
    Protocol.Runtime.evaluate({ expression: "console.log(239) "}).then(next);
  },

  function consoleTimeWithCommaAsSeparator(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    setlocale("fr_CA.UTF-8");
    Protocol.Runtime.evaluate({ expression: "console.time(\"a\"); console.timeEnd(\"a\")"}).then(next);
  },

  function consoleLogWithCommaAsSeparator(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    setlocale("fr_CA.UTF-8");
    Protocol.Runtime.evaluate({ expression: "console.log(239) "}).then(next);
  },

  function consoleTimeWithCommaAfterConsoleLog(next) {
    InspectorTest.log("set locale to fr_CA.UTF-8 (has comma as separator)");
    setlocale("fr_CA.UTF-8");
    Protocol.Runtime.evaluate({ expression: "console.log(239) "})
      .then(() => Protocol.Runtime.evaluate({ expression: "console.time(\"a\"); console.timeEnd(\"a\")"}))
      .then(next);
  }
]);

function dumpConsoleApiCalled(message) {
  var firstArg = message.params.args[0];
  if (firstArg.type === "string")
    firstArg.value = firstArg.value.replace(/[0-9]+/g, "x");
  InspectorTest.logMessage(message);
}
