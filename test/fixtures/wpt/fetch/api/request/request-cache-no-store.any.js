// META: global=window,worker
// META: title=Request cache - no store
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=request-cache.js

var tests = [
  {
    name: 'RequestCache "no-store" mode does not check the cache for previously cached content and goes to the network regardless',
    state: "stale",
    request_cache: ["default", "no-store"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, true],
  },
  {
    name: 'RequestCache "no-store" mode does not check the cache for previously cached content and goes to the network regardless',
    state: "fresh",
    request_cache: ["default", "no-store"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, true],
  },
  {
    name: 'RequestCache "no-store" mode does not store the response in the cache',
    state: "stale",
    request_cache: ["no-store", "default"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [true, false],
  },
  {
    name: 'RequestCache "no-store" mode does not store the response in the cache',
    state: "fresh",
    request_cache: ["no-store", "default"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [true, false],
  },
];
run_tests(tests);
