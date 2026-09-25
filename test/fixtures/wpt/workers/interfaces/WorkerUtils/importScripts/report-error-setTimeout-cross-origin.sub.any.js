// META: global=dedicatedworker,sharedworker
// META: script=report-error-helper.js
const crossOrigin = "https://{{hosts[alt][]}}:{{ports[https][0]}}";
runTest(
  crossOrigin + "/workers/modules/resources/syntax-error.js",
  true,
  "NetworkError"
);
