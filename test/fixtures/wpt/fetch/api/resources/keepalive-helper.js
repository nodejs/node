// Utility functions to help testing keepalive requests.

// Returns a URL to an iframe that loads a keepalive URL on iframe loaded.
//
// The keepalive URL points to a target that stores `token`. The token will then
// be posted back on iframe loaded to the parent document.
// `method` defaults to GET.
// `frameOrigin` to specify the origin of the iframe to load. If not set,
// default to a different site origin.
// `requestOrigin` to specify the origin of the fetch request target.
// `sendOn` to specify the name of the event when the keepalive request should
// be sent instead of the default 'load'.
// `mode` to specify the fetch request's CORS mode.
// `disallowCrossOrigin` to ask the iframe to set up a server that disallows
// cross origin requests.
function getKeepAliveIframeUrl(token, method, {
  frameOrigin = 'DEFAULT',
  requestOrigin = '',
  sendOn = 'load',
  mode = 'cors',
  disallowCrossOrigin = false
} = {}) {
  const https = location.protocol.startsWith('https');
  frameOrigin = frameOrigin === 'DEFAULT' ?
      get_host_info()[https ? 'HTTPS_NOTSAMESITE_ORIGIN' : 'HTTP_NOTSAMESITE_ORIGIN'] :
      frameOrigin;
  return `${frameOrigin}/fetch/api/resources/keepalive-iframe.html?` +
      `token=${token}&` +
      `method=${method}&` +
      `sendOn=${sendOn}&` +
      `mode=${mode}&` + (disallowCrossOrigin ? `disallowCrossOrigin=1&` : ``) +
      `origin=${requestOrigin}`;
}

// Returns a different-site URL to an iframe that loads a keepalive URL.
//
// By default, the keepalive URL points to a target that redirects to another
// same-origin destination storing `token`. The token will then be posted back
// to parent document.
//
// The URL redirects can be customized from `origin1` to `origin2` if provided.
// Sets `withPreflight` to true to get URL enabling preflight.
function getKeepAliveAndRedirectIframeUrl(
    token, origin1, origin2, withPreflight) {
  const https = location.protocol.startsWith('https');
  const frameOrigin =
      get_host_info()[https ? 'HTTPS_NOTSAMESITE_ORIGIN' : 'HTTP_NOTSAMESITE_ORIGIN'];
  return `${frameOrigin}/fetch/api/resources/keepalive-redirect-iframe.html?` +
      `token=${token}&` +
      `origin1=${origin1}&` +
      `origin2=${origin2}&` + (withPreflight ? `with-headers` : ``);
}

async function iframeLoaded(iframe) {
  return new Promise((resolve) => iframe.addEventListener('load', resolve));
}

// Obtains the token from the message posted by iframe after loading
// `getKeepAliveAndRedirectIframeUrl()`.
async function getTokenFromMessage() {
  return new Promise((resolve) => {
    window.addEventListener('message', (event) => {
      resolve(event.data);
    }, {once: true});
  });
}

// Tells if `token` has been stored in the server.
async function queryToken(token) {
  const response = await fetch(`../resources/stash-take.py?key=${token}`);
  const json = await response.json();
  return json;
}

// A helper to assert the existence of `token` that should have been stored in
// the server by fetching ../resources/stash-put.py.
//
// This function simply wait for a custom amount of time before trying to
// retrieve `token` from the server.
// `expectTokenExist` tells if `token` should be present or not.
//
// NOTE:
// In order to parallelize the work, we are going to have an async_test
// for the rest of the work. Note that we want the serialized behavior
// for the steps so far, so we don't want to make the entire test case
// an async_test.
function assertStashedTokenAsync(
    testName, token, {expectTokenExist = true} = {}) {
  async_test(test => {
    new Promise(resolve => test.step_timeout(resolve, 3000 /*ms*/))
        .then(test.step_func(() => {
          return queryToken(token);
        }))
        .then(test.step_func(result => {
          if (expectTokenExist) {
            assert_equals(result, 'on', `token should be on (stashed).`);
            test.done();
          } else {
            assert_not_equals(
                result, 'on', `token should not be on (stashed).`);
            return Promise.reject(`Failed to retrieve token from server`);
          }
        }))
        .catch(test.step_func(e => {
          if (expectTokenExist) {
            test.unreached_func(e);
          } else {
            test.done();
          }
        }));
  }, testName);
}

/**
 * In an iframe, and in `load` event handler, test to fetch a keepalive URL that
 * involves in redirect to another URL.
 *
 * `unloadIframe` to unload the iframe before verifying stashed token to
 * simulate the situation that unloads after fetching. Note that this test is
 * different from `keepaliveRedirectInUnloadTest()` in that the latter
 * performs fetch() call directly in `unload` event handler, while this test
 * does it in `load`.
 */
function keepaliveRedirectTest(desc, {
  origin1 = '',
  origin2 = '',
  withPreflight = false,
  unloadIframe = false,
  expectFetchSucceed = true,
} = {}) {
  desc = `[keepalive][iframe][load] ${desc}` +
      (unloadIframe ? ' [unload at end]' : '');
  promise_test(async (test) => {
    const tokenToStash = token();
    const iframe = document.createElement('iframe');
    iframe.src = getKeepAliveAndRedirectIframeUrl(
        tokenToStash, origin1, origin2, withPreflight);
    document.body.appendChild(iframe);
    await iframeLoaded(iframe);
    assert_equals(await getTokenFromMessage(), tokenToStash);
    if (unloadIframe) {
      iframe.remove();
    }

    assertStashedTokenAsync(
        desc, tokenToStash, {expectTokenExist: expectFetchSucceed});
  }, `${desc}; setting up`);
}

/**
 * Opens a different site window, and in `unload` event handler, test to fetch
 * a keepalive URL that involves in redirect to another URL.
 */
function keepaliveRedirectInUnloadTest(desc, {
  origin1 = '',
  origin2 = '',
  url2 = '',
  withPreflight = false,
  expectFetchSucceed = true
} = {}) {
  desc = `[keepalive][new window][unload] ${desc}`;

  promise_test(async (test) => {
    const targetUrl =
        `${HTTP_NOTSAMESITE_ORIGIN}/fetch/api/resources/keepalive-redirect-window.html?` +
        `origin1=${origin1}&` +
        `origin2=${origin2}&` +
        `url2=${url2}&` + (withPreflight ? `with-headers` : ``);
    const w = window.open(targetUrl);
    const token = await getTokenFromMessage();
    w.close();

    assertStashedTokenAsync(
        desc, token, {expectTokenExist: expectFetchSucceed});
  }, `${desc}; setting up`);
}

/**
* utility to create pending keepalive fetch requests
* The pending request state is achieved by ensuring the server (trickle.py) does not
* immediately respond to the fetch requests.
* The response delay is set as a url parameter.
*/

function createPendingKeepAliveRequest(delay, remote = false) {
  // trickle.py is a script that can make a delayed response to the client request
  const trickleRemoteURL = get_host_info().HTTPS_REMOTE_ORIGIN + '/fetch/api/resources/trickle.py?count=1&ms=';
  const trickleLocalURL = get_host_info().HTTP_ORIGIN + '/fetch/api/resources/trickle.py?count=1&ms=';
  url = remote ? trickleRemoteURL : trickleLocalURL;

  const body = '*'.repeat(10);
  return fetch(url + delay, { keepalive: true, body, method: 'POST' }).then(res => {
      return res.text();
  }).then(() => {
      return new Promise(resolve => step_timeout(resolve, 1));
  }).catch((error) => {
      return Promise.reject(error);;
  })
}
