// META: global=dedicatedworker,sharedworker
// META: script=report-error-helper.js
runTest(
  "/workers/modules/resources/syntax-error.js",
  true,
  "SyntaxError"
);
