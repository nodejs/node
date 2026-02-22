// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

const {
  HTTP_ORIGIN,
  HTTP_REMOTE_ORIGIN,
} = get_host_info();

/**
 * Fetches `fromUrl` with 'cors' and 'follow' modes that returns response to
 * redirect to `toUrl`.
 */
function testOriginAfterRedirection(
    desc, method, fromUrl, toUrl, statusCode, expectedOrigin) {
  desc = `[${method}] Redirect ${statusCode} ${desc}`;
  const token1 = token();
  const url = `${fromUrl}?token=${token1}&max_age=0` +
      `&redirect_status=${statusCode}` +
      `&location=${encodeURIComponent(toUrl)}`;

  const requestInit = {method, 'mode': 'cors', 'redirect': 'follow'};

  promise_test(function(test) {
    return fetch(`${RESOURCES_DIR}clean-stash.py?token=${token1}`)
        .then((cleanResponse) => {
          assert_equals(
              cleanResponse.status, 200,
              `Clean stash response's status is 200`);
          return fetch(url, requestInit).then((redirectResponse) => {
            assert_equals(
                redirectResponse.status, 200,
                `Inspect header response's status is 200`);
            assert_equals(
                redirectResponse.headers.get('x-request-origin'),
                expectedOrigin, 'Check origin header');
          });
        });
  }, desc);
}

const FROM_URL = `${RESOURCES_DIR}redirect.py`;
const CORS_FROM_URL =
    `${HTTP_REMOTE_ORIGIN}${dirname(location.pathname)}${FROM_URL}`;
const TO_URL = `${HTTP_ORIGIN}${dirname(location.pathname)}${
    RESOURCES_DIR}inspect-headers.py?headers=origin`;
const CORS_TO_URL = `${HTTP_REMOTE_ORIGIN}${dirname(location.pathname)}${
    RESOURCES_DIR}inspect-headers.py?cors&headers=origin`;

for (const statusCode of [301, 302, 303, 307, 308]) {
  for (const method of ['GET', 'POST']) {
    testOriginAfterRedirection(
        'Same origin to same origin', method, FROM_URL, TO_URL, statusCode,
        null);
    testOriginAfterRedirection(
        'Same origin to other origin', method, FROM_URL, CORS_TO_URL,
        statusCode, HTTP_ORIGIN);
    testOriginAfterRedirection(
        'Other origin to other origin', method, CORS_FROM_URL, CORS_TO_URL,
        statusCode, HTTP_ORIGIN);
    // TODO(crbug.com/1432059): Fix broken tests.
    testOriginAfterRedirection(
        'Other origin to same origin', method, CORS_FROM_URL, `${TO_URL}&cors`,
        statusCode, 'null');
  }
}

done();
