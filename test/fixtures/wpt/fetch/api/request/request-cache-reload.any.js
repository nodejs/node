// META: global=window,worker
// META: title=Request cache - reload
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=request-cache.js

var tests = [
  {
    name: 'RequestCache "reload" mode does not check the cache for previously cached content and goes to the network regardless',
    state: "stale",
    request_cache: ["default", "reload"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, true],
  },
  {
    name: 'RequestCache "reload" mode does not check the cache for previously cached content and goes to the network regardless',
    state: "fresh",
    request_cache: ["default", "reload"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, true],
  },
  {
    name: 'RequestCache "reload" mode does store the response in the cache',
    state: "stale",
    request_cache: ["reload", "default"],
    expected_validation_headers: [false, true],
    expected_no_cache_headers: [true, false],
  },
  {
    name: 'RequestCache "reload" mode does store the response in the cache',
    state: "fresh",
    request_cache: ["reload", "default"],
    expected_validation_headers: [false],
    expected_no_cache_headers: [true],
  },
  {
    name: 'RequestCache "reload" mode does store the response in the cache even if a previous response is already stored',
    state: "stale",
    request_cache: ["default", "reload", "default"],
    expected_validation_headers: [false, false, true],
    expected_no_cache_headers: [false, true, false],
  },
  {
    name: 'RequestCache "reload" mode does store the response in the cache even if a previous response is already stored',
    state: "fresh",
    request_cache: ["default", "reload", "default"],
    expected_validation_headers: [false, false],
    expected_no_cache_headers: [false, true],
  },
];
run_tests(tests);
