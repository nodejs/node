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


keepaliveRedirectInUnloadTest('same-origin redirect');
keepaliveRedirectInUnloadTest(
    'same-origin redirect + preflight', {withPreflight: true});
keepaliveRedirectInUnloadTest('cross-origin redirect', {
  origin1: HTTP_REMOTE_ORIGIN,
  origin2: HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT
});
keepaliveRedirectInUnloadTest('cross-origin redirect + preflight', {
  origin1: HTTP_REMOTE_ORIGIN,
  origin2: HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT,
  withPreflight: true
});
keepaliveRedirectInUnloadTest(
    'redirect to file URL',
    {url2: 'file://tmp/bar.txt', expectFetchSucceed: false});
keepaliveRedirectInUnloadTest('redirect to data URL', {
  url2: 'data:text/plain;base64,cmVzcG9uc2UncyBib2R5',
  expectFetchSucceed: false
});
