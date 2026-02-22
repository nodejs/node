// META: global=window,worker
// META: title=Request cache : no-cache
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=request-cache.js

var tests = [
  {
    name: 'RequestCache "no-cache" mode revalidates stale responses found in the cache',
    state: "stale",
    request_cache: ["default", "no-cache"],
    expected_validation_headers: [false, true],
    expected_no_cache_headers: [false, false],
    expected_max_age_headers: [false, true],
  },
  {
    name: 'RequestCache "no-cache" mode revalidates fresh responses found in the cache',
    state: "fresh",
    request_cache: ["default", "no-cache"],
    expected_validation_headers: [false, true],
    expected_no_cache_headers: [false, false],
    expected_max_age_headers: [false, true],
  },
];
run_tests(tests);
