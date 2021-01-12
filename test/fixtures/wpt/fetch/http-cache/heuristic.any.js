// META: global=window,worker
// META: title=HTTP Cache - Heuristic Freshness
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: "HTTP cache reuses an unknown response with Last-Modified based upon heuristic freshness when Cache-Control: public is present",
    requests: [
      {
        response_status: [299, "Whatever"],
        response_headers: [
          ["Last-Modified", (-3 * 100)],
          ["Cache-Control", "public"]
        ],
      },
      {
        expected_type: "cached",
        response_status: [299, "Whatever"]
      }
    ]
  },
  {
    name: "HTTP cache does not reuse an unknown response with Last-Modified based upon heuristic freshness when Cache-Control: public is not present",
    requests: [
      {
        response_status: [299, "Whatever"],
        response_headers: [
          ["Last-Modified", (-3 * 100)]
        ],
      },
      {
        expected_type: "not_cached"
      }
    ]
  }
];

function check_status(status) {
  var succeed = status[0];
  var code = status[1];
  var phrase = status[2];
  var body = status[3];
  if (body === undefined) {
    body = http_content(code);
  }
  var expected_type = "not_cached";
  var desired = "does not use"
  if (succeed === true) {
    expected_type = "cached";
    desired = "reuses";
  }
  tests.push(
    {
      name: "HTTP cache " + desired + " a " + code + " " + phrase + " response with Last-Modified based upon heuristic freshness",
      requests: [
        {
          response_status: [code, phrase],
          response_headers: [
            ["Last-Modified", (-3 * 100)]
          ],
          response_body: body
        },
        {
          expected_type: expected_type,
          response_status: [code, phrase],
          response_body: body
        }
      ]
    }
  )
}
[
  [true, 200, "OK"],
  [true, 203, "Non-Authoritative Information"],
  [true, 204, "No Content", ""],
  [true, 404, "Not Found"],
  [true, 405, "Method Not Allowed"],
  [true, 410, "Gone"],
  [true, 414, "URI Too Long"],
  [true, 501, "Not Implemented"]
].forEach(check_status);
[
  [false, 201, "Created"],
  [false, 202, "Accepted"],
  [false, 403, "Forbidden"],
  [false, 502, "Bad Gateway"],
  [false, 503, "Service Unavailable"],
  [false, 504, "Gateway Timeout"],
].forEach(check_status);
run_tests(tests);
