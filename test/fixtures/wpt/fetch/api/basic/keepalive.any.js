// META: global=window
// META: timeout=long
// META: title=Fetch API: keepalive handling
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=../resources/keepalive-helper.js

'use strict';

const {
  HTTP_NOTSAMESITE_ORIGIN,
  HTTP_REMOTE_ORIGIN,
  HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT
} = get_host_info();

/**
 * In a different-site iframe, test to fetch a keepalive URL on the specified
 * document event.
 */
function keepaliveSimpleRequestTest(method) {
  for (const evt of ['load', 'unload', 'pagehide']) {
    const desc =
        `[keepalive] simple ${method} request on '${evt}' [no payload]`;
    promise_test(async (test) => {
      const token1 = token();
      const iframe = document.createElement('iframe');
      iframe.src = getKeepAliveIframeUrl(token1, method, {sendOn: evt});
      document.body.appendChild(iframe);
      await iframeLoaded(iframe);
      if (evt != 'load') {
        iframe.remove();
      }

      assertStashedTokenAsync(desc, token1);
    }, `${desc}; setting up`);
  }
}

for (const method of ['GET', 'POST']) {
  keepaliveSimpleRequestTest(method);
}

// verifies fetch keepalive requests from a worker
function keepaliveSimpleWorkerTest() {
    const desc =
        `simple keepalive test for web workers`;
    promise_test(async (test) => {
      const TOKEN = token();
      const FRAME_ORIGIN = new URL(location.href).origin;
      const TEST_URL = get_host_info().HTTP_ORIGIN + `/fetch/api/resources/stash-put.py?key=${TOKEN}&value=on`
    + `&frame_origin=${FRAME_ORIGIN}`;
      // start a worker which sends keepalive request and immediately terminates
      const worker = new Worker(`/fetch/api/resources/keepalive-worker.js?param=${TEST_URL}`);

      const keepAliveWorkerPromise = new Promise((resolve, reject) => {
        worker.onmessage = (event) => {
          if (event.data === 'started') {
            resolve();
          } else {
            reject(new Error("Unexpected message received from worker"));
          }
        };
        worker.onerror = (error) => {
          reject(error);
        };
      });

      // wait until the worker has been initialized (indicated by the "started" message)
      await keepAliveWorkerPromise;
      // verifies if the token sent in fetch request has been updated in the server
      assertStashedTokenAsync(desc, TOKEN);

    }, `${desc};`);

}

keepaliveSimpleWorkerTest();
