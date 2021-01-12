// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=resources/corspreflight.js

const corsURL = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";

promise_test(() => fetch("resources/not-cors-safelisted.json").then(res => res.json().then(runTests)), "Loading data…");

function runTests(testArray) {
  testArray.forEach(testItem => {
    const [headerName, headerValue] = testItem;
    corsPreflight("Need CORS-preflight for " + headerName + "/" + headerValue + " header",
                  corsURL,
                  "GET",
                  true,
                  [[headerName, headerValue]]);
  });
}
