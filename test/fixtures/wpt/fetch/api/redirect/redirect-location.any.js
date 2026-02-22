// META: global=window,worker
// META: script=../resources/utils.js

const VALID_URL = 'top.txt';
const INVALID_URL = 'invalidurl:';
const DATA_URL = 'data:text/plain;base64,cmVzcG9uc2UncyBib2R5';

/**
 *  A test to fetch a URL that returns response redirecting to `toUrl` with
 * `status` as its HTTP status code. `expectStatus` can be set to test the
 * status code in fetch's Promise response.
 */
function redirectLocationTest(toUrlDesc, {
  toUrl = undefined,
  status,
  expectStatus = undefined,
  mode,
  shouldPass = true
} = {}) {
  toUrlDesc = toUrl ? `with ${toUrlDesc}` : `without`;
  const desc = `Redirect ${status} in "${mode}" mode ${toUrlDesc} location`;
  const url = `${RESOURCES_DIR}redirect.py?redirect_status=${status}` +
      (toUrl ? `&location=${encodeURIComponent(toUrl)}` : '');
  const requestInit = {'redirect': mode};
  if (!expectStatus)
    expectStatus = status;

  promise_test((test) => {
    if (mode === 'error' || !shouldPass)
      return promise_rejects_js(test, TypeError, fetch(url, requestInit));
    if (mode === 'manual')
      return fetch(url, requestInit).then((resp) => {
        assert_equals(resp.status, 0, "Response's status is 0");
        assert_equals(resp.type, "opaqueredirect", "Response's type is opaqueredirect");
        assert_equals(resp.statusText, '', `Response's statusText is ""`);
        assert_true(resp.headers.entries().next().done, "Headers should be empty");
      });

    if (mode === 'follow')
      return fetch(url, requestInit).then((resp) => {
        assert_equals(
            resp.status, expectStatus, `Response's status is ${expectStatus}`);
      });
    assert_unreached(`${mode} is not a valid redirect mode`);
  }, desc);
}

// FIXME: We may want to mix redirect-mode and cors-mode.
for (const status of [301, 302, 303, 307, 308]) {
  redirectLocationTest('without location', {status, mode: 'follow'});
  redirectLocationTest('without location', {status, mode: 'manual'});
  // FIXME: Add tests for "error" redirect-mode without location.

  // When succeeded, `follow` mode should have followed all redirects.
  redirectLocationTest(
      'valid', {toUrl: VALID_URL, status, expectStatus: 200, mode: 'follow'});
  redirectLocationTest('valid', {toUrl: VALID_URL, status, mode: 'manual'});
  redirectLocationTest('valid', {toUrl: VALID_URL, status, mode: 'error'});

  redirectLocationTest(
      'invalid',
      {toUrl: INVALID_URL, status, mode: 'follow', shouldPass: false});
  redirectLocationTest('invalid', {toUrl: INVALID_URL, status, mode: 'manual'});
  redirectLocationTest('invalid', {toUrl: INVALID_URL, status, mode: 'error'});

  redirectLocationTest(
      'data', {toUrl: DATA_URL, status, mode: 'follow', shouldPass: false});
  // FIXME: Should this pass?
  redirectLocationTest('data', {toUrl: DATA_URL, status, mode: 'manual'});
  redirectLocationTest('data', {toUrl: DATA_URL, status, mode: 'error'});
}

done();
