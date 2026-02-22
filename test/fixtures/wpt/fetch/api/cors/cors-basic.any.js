// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

const {
  HTTPS_ORIGIN,
  HTTP_ORIGIN_WITH_DIFFERENT_PORT,
  HTTP_REMOTE_ORIGIN,
  HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT,
  HTTPS_REMOTE_ORIGIN,
} = get_host_info();

function cors(desc, origin) {
  const url = `${origin}${dirname(location.pathname)}${RESOURCES_DIR}top.txt`;
  const urlAllowCors = `${url}?pipe=header(Access-Control-Allow-Origin,*)`;

  promise_test((test) => {
    return fetch(urlAllowCors, {'mode': 'no-cors'}).then((resp) => {
      assert_equals(resp.status, 0, "Opaque filter: status is 0");
      assert_equals(resp.statusText, "", "Opaque filter: statusText is \"\"");
      assert_equals(resp.type , "opaque", "Opaque filter: response's type is opaque");
      return resp.text().then((value) => {
        assert_equals(value, "", "Opaque response should have an empty body");
      });
    });
  }, `${desc} [no-cors mode]`);

  promise_test((test) => {
    return promise_rejects_js(test, TypeError, fetch(url, {'mode': 'cors'}));
  }, `${desc} [server forbid CORS]`);

  promise_test((test) => {
    return fetch(urlAllowCors, {'mode': 'cors'}).then((resp) => {
      assert_equals(resp.status, 200, "Fetch's response's status is 200");
      assert_equals(resp.type , "cors", "CORS response's type is cors");
    });
  }, `${desc} [cors mode]`);
}

cors('Same domain different port', HTTP_ORIGIN_WITH_DIFFERENT_PORT);
cors('Same domain different protocol different port', HTTPS_ORIGIN);
cors('Cross domain basic usage', HTTP_REMOTE_ORIGIN);
cors('Cross domain different port', HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT);
cors('Cross domain different protocol', HTTPS_REMOTE_ORIGIN);
