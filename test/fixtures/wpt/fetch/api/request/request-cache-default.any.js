// META: global=window,worker
// META: title=Request cache - default
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=request-cache.js

var tests = [
  {
    name: 'RequestCache "default" mode checks the cache for previously cached content and goes to the network for stale responses',
    state: "stale",
    request_cache: ["default", "default"],
    expected_validation_headers: [false, true],
    expected_no_cache_headers: [false, false],
  },
  {
    name: 'RequestCache "default" mode checks the cache for previously cached content and avoids going to the network if a fresh response exists',
    state: "fresh",
    request_cache: ["default", "default"],
    expected_validation_headers: [false],
    expected_no_cache_headers: [false],
  },
  {
    name: 'Responses with the "Cache-Control: no-store" header are not stored in the cache',
    state: "stale",
    cache_control: "no-store",
    request_cache: ["default", "default"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, false],
  },
  {
    name: 'Responses with the "Cache-Control: no-store" header are not stored in the cache',
    state: "fresh",
    cache_control: "no-store",
    request_cache: ["default", "default"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, false],
  },
];
run_tests(tests);
