// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

var cors_url = get_host_info().HTTP_REMOTE_ORIGIN +
              dirname(location.pathname) +
              RESOURCES_DIR +
              "preflight.py";

promise_test((test) => {
  var uuid_token = token();
  var request_url =
      cors_url + "?token=" + uuid_token + "&max_age=12000&allow_methods=POST" +
      "&allow_headers=x-test-header";
  return fetch(cors_url + "?token=" + uuid_token + "&clear-stash")
    .then(() => {
      return fetch(
        new Request(request_url,
                    {
                      mode: "cors",
                      method: "POST",
                      headers: [["x-test-header", "test1"]]
                    }));
    })
    .then((resp) => {
      assert_equals(resp.status, 200, "Response's status is 200");
      assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
      return fetch(cors_url + "?token=" + uuid_token + "&clear-stash");
    })
    .then((res) => res.text())
    .then((txt) => {
      assert_equals(txt, "1", "Server stash must be cleared.");
      return fetch(
        new Request(request_url,
                    {
                      mode: "cors",
                      method: "POST",
                      headers: [["x-test-header", "test2"]]
                    }));
    })
    .then((resp) => {
      assert_equals(resp.status, 200, "Response's status is 200");
      assert_equals(resp.headers.get("x-did-preflight"), "0", "Preflight request has not been made");
      return fetch(cors_url + "?token=" + uuid_token + "&clear-stash");
    });
});
