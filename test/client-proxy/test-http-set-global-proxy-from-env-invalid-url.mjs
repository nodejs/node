// Tests that http.setGlobalProxyFromEnv() throws on invalid HTTP proxy URL.

import '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';

assert.throws(() => {
  http.setGlobalProxyFromEnv({
    http_proxy: 'not a url',
  });
}, {
  code: 'ERR_PROXY_INVALID_CONFIG',
});

assert.throws(() => {
  http.setGlobalProxyFromEnv({
    https_proxy: 'not a url',
  });
}, {
  code: 'ERR_PROXY_INVALID_CONFIG',
});
