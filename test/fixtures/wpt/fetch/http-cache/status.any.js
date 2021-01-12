// META: global=window,worker
// META: title=HTTP Cache - Status Codes
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [];
function check_status(status) {
  var code = status[0];
  var phrase = status[1];
  var body = status[2];
  if (body === undefined) {
    body = http_content(code);
  }
  tests.push({
    name: "HTTP cache goes to the network if it has a stale " + code + " response",
    requests: [
      {
        template: "stale",
        response_status: [code, phrase],
        response_body: body
      }, {
        expected_type: "not_cached",
        response_status: [code, phrase],
        response_body: body
      }
    ]
  })
  tests.push({
    name: "HTTP cache avoids going to the network if it has a fresh " + code + " response",
    requests: [
      {
        template: "fresh",
        response_status: [code, phrase],
        response_body: body
      }, {
        expected_type: "cached",
        response_status: [code, phrase],
        response_body: body
      }
    ]
  })
}
[
  [200, "OK"],
  [203, "Non-Authoritative Information"],
  [204, "No Content", null],
  [299, "Whatever"],
  [400, "Bad Request"],
  [404, "Not Found"],
  [410, "Gone"],
  [499, "Whatever"],
  [500, "Internal Server Error"],
  [502, "Bad Gateway"],
  [503, "Service Unavailable"],
  [504, "Gateway Timeout"],
  [599, "Whatever"]
].forEach(check_status);
run_tests(tests);
