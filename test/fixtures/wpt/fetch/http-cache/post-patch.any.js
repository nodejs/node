// META: global=window,worker
// META: title=HTTP Cache - Caching POST and PATCH responses
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: "HTTP cache uses content after PATCH request with response containing Content-Location and cache-allowing header",
    requests: [
      {
        request_method: "PATCH",
        request_body: "abc",
        response_status: [200, "OK"],
        response_headers: [
          ['Cache-Control', "private, max-age=1000"],
          ['Content-Location', ""]
        ],
        response_body: "abc"
      },
      {
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache uses content after POST request with response containing Content-Location and cache-allowing header",
    requests: [
      {
        request_method: "POST",
        request_body: "abc",
        response_status: [200, "OK"],
        response_headers: [
          ['Cache-Control', "private, max-age=1000"],
          ['Content-Location', ""]
        ],
        response_body: "abc"
      },
      {
        expected_type: "cached"
      }
    ]
  }
];
run_tests(tests);
