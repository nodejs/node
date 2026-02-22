// META: global=window,worker
// META: script=../resources/utils.js
// META: script=/common/utils.js
// META: timeout=long

/**
 * Fetches a target that returns response with HTTP status code `statusCode` to
 * redirect `maxCount` times.
 */
function redirectCountTest(maxCount, {statusCode, shouldPass = true} = {}) {
  const desc = `Redirect ${statusCode} ${maxCount} times`;

  const fromUrl = `${RESOURCES_DIR}redirect.py`;
  const toUrl = fromUrl;
  const token1 = token();
  const url = `${fromUrl}?token=${token1}` +
      `&max_age=0` +
      `&redirect_status=${statusCode}` +
      `&max_count=${maxCount}` +
      `&location=${encodeURIComponent(toUrl)}`;

  const requestInit = {'redirect': 'follow'};

  promise_test((test) => {
    return fetch(`${RESOURCES_DIR}clean-stash.py?token=${token1}`)
        .then((resp) => {
          assert_equals(
              resp.status, 200, 'Clean stash response\'s status is 200');

          if (!shouldPass)
            return promise_rejects_js(test, TypeError, fetch(url, requestInit));

          return fetch(url, requestInit)
              .then((resp) => {
                assert_equals(resp.status, 200, 'Response\'s status is 200');
                return resp.text();
              })
              .then((body) => {
                assert_equals(
                    body, maxCount.toString(), `Redirected ${maxCount} times`);
              });
        });
  }, desc);
}

for (const statusCode of [301, 302, 303, 307, 308]) {
  redirectCountTest(20, {statusCode});
  redirectCountTest(21, {statusCode, shouldPass: false});
}

done();
