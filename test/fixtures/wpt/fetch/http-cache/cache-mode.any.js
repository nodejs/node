// META: global=window,worker
// META: title=Fetch - Cache Mode
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: "Fetch sends Cache-Control: max-age=0 when cache mode is no-cache",
    requests: [
      {
        cache: "no-cache",
        expected_request_headers: [['cache-control', 'max-age=0']]
      }
    ]
  },
  {
    name: "Fetch doesn't touch Cache-Control when cache mode is no-cache and Cache-Control is already present",
    requests: [
      {
        cache: "no-cache",
        request_headers: [['cache-control', 'foo']],
        expected_request_headers: [['cache-control', 'foo']]
      }
    ]
  },
  {
    name: "Fetch sends Cache-Control: no-cache and Pragma: no-cache when cache mode is no-store",
    requests: [
      {
        cache: "no-store",
        expected_request_headers: [
          ['cache-control', 'no-cache'],
          ['pragma', 'no-cache']
        ]
      }
    ]
  },
  {
    name: "Fetch doesn't touch Cache-Control when cache mode is no-store and Cache-Control is already present",
    requests: [
      {
        cache: "no-store",
        request_headers: [['cache-control', 'foo']],
        expected_request_headers: [['cache-control', 'foo']]
      }
    ]
  },
  {
    name: "Fetch doesn't touch Pragma when cache mode is no-store and Pragma is already present",
    requests: [
      {
        cache: "no-store",
        request_headers: [['pragma', 'foo']],
        expected_request_headers: [['pragma', 'foo']]
      }
    ]
  }
];
run_tests(tests);
