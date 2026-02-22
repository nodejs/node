// META: global=window
// META: timeout=long
// META: title=Fetch API: keepalive handling
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=../resources/keepalive-helper.js
// META: script=../resources/utils.js

'use strict';

const {
  HTTP_NOTSAMESITE_ORIGIN,
  HTTPS_ORIGIN,
  HTTP_ORIGIN_WITH_DIFFERENT_PORT,
  HTTP_REMOTE_ORIGIN,
  HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT,
  HTTPS_REMOTE_ORIGIN,
} = get_host_info();

/**
 * Tests to cover the basic behaviors of keepalive + cors/no-cors mode requests
 * to different `origin` when the initiator document is still alive. They should
 * behave the same as without setting keepalive.
 */
function keepaliveCorsBasicTest(desc, origin) {
  const url = `${origin}${dirname(location.pathname)}${RESOURCES_DIR}top.txt`;
  const urlAllowCors = `${url}?pipe=header(Access-Control-Allow-Origin,*)`;

  promise_test((test) => {
    return fetch(urlAllowCors, {keepalive: true, 'mode': 'no-cors'})
        .then((resp) => {
          assert_equals(resp.status, 0, 'Opaque filter: status is 0');
          assert_equals(resp.statusText, '', 'Opaque filter: statusText is ""');
          assert_equals(
              resp.type, 'opaque', 'Opaque filter: response\'s type is opaque');
          return resp.text().then((value) => {
            assert_equals(
                value, '', 'Opaque response should have an empty body');
          });
        });
  }, `${desc} [no-cors mode]`);

  promise_test((test) => {
    return promise_rejects_js(
        test, TypeError, fetch(url, {keepalive: true, 'mode': 'cors'}));
  }, `${desc} [cors mode, server forbid CORS]`);

  promise_test((test) => {
    return fetch(urlAllowCors, {keepalive: true, 'mode': 'cors'})
        .then((resp) => {
          assert_equals(resp.status, 200, 'Fetch\'s response\'s status is 200');
          assert_equals(resp.type, 'cors', 'CORS response\'s type is cors');
        });
  }, `${desc} [cors mode]`);
}

keepaliveCorsBasicTest(
    `[keepalive] Same domain different port`, HTTP_ORIGIN_WITH_DIFFERENT_PORT);
keepaliveCorsBasicTest(
    `[keepalive] Same domain different protocol different port`, HTTPS_ORIGIN);
keepaliveCorsBasicTest(
    `[keepalive] Cross domain basic usage`, HTTP_REMOTE_ORIGIN);
keepaliveCorsBasicTest(
    `[keepalive] Cross domain different port`,
    HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT);
keepaliveCorsBasicTest(
    `[keepalive] Cross domain different protocol`, HTTPS_REMOTE_ORIGIN);

/**
 * In a same-site iframe, and in `unload` event handler, test to fetch
 * a keepalive URL that involves in different cors modes.
 */
function keepaliveCorsInUnloadTest(description, origin, method) {
  const evt = 'unload';
  for (const mode of ['no-cors', 'cors']) {
    for (const disallowCrossOrigin of [false, true]) {
      const desc = `${description} ${method} request in ${evt} [${mode} mode` +
          (disallowCrossOrigin ? ']' : ', server forbid CORS]');
      const expectTokenExist = !disallowCrossOrigin || mode === 'no-cors';
      promise_test(async (test) => {
        const token1 = token();
        const iframe = document.createElement('iframe');
        iframe.src = getKeepAliveIframeUrl(token1, method, {
          frameOrigin: '',
          requestOrigin: origin,
          sendOn: evt,
          mode: mode,
          disallowCrossOrigin
        });
        document.body.appendChild(iframe);
        await iframeLoaded(iframe);
        iframe.remove();
        assert_equals(await getTokenFromMessage(), token1);

        assertStashedTokenAsync(desc, token1, {expectTokenExist});
      }, `${desc}; setting up`);
    }
  }
}

for (const method of ['GET', 'POST']) {
  keepaliveCorsInUnloadTest(
      '[keepalive] Same domain different port', HTTP_ORIGIN_WITH_DIFFERENT_PORT,
      method);
  keepaliveCorsInUnloadTest(
      `[keepalive] Same domain different protocol different port`, HTTPS_ORIGIN,
      method);
  keepaliveCorsInUnloadTest(
      `[keepalive] Cross domain basic usage`, HTTP_REMOTE_ORIGIN, method);
  keepaliveCorsInUnloadTest(
      `[keepalive] Cross domain different port`,
      HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT, method);
  keepaliveCorsInUnloadTest(
      `[keepalive] Cross domain different protocol`, HTTPS_REMOTE_ORIGIN,
      method);
}
