'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { spawnSync } = require('child_process');

// Regression test for https://github.com/nodejs/node/issues/66202
// fetch() and http.request() must agree on how to treat an explicit
// empty string in a lower-cased proxy env var.

if (!common.hasCrypto) common.skip('missing crypto');

const proxy = http.createServer((req, res) => {
  res.setHeader('x-via-proxy', '1');
  res.end('ok');
});

proxy.listen(0, common.mustCall(() => {
  const proxyUrl = `http://localhost:${proxy.address().port}`;

  const script = `
    const assert = require('assert');
    (async () => {
      const usesProxyRequest = await new Promise((resolve) => {
        require('http').get('http://localhost:1/', (res) => {
          resolve(res.headers['x-via-proxy'] === '1');
        }).on('error', () => resolve(false));
      });

      let usesProxyFetch = false;
      try {
        const res = await fetch('http://localhost:1/');
        usesProxyFetch = res.headers.get('x-via-proxy') === '1';
      } catch { /* direct connection refused is expected if no proxy used */ }

      assert.strictEqual(usesProxyFetch, usesProxyRequest,
        'fetch() and http.request() disagree on proxy usage');
    })();
  `;

  const result = spawnSync(process.execPath, ['--use-env-proxy', '-e', script], {
    env: {
      ...process.env,
      http_proxy: '',
      HTTP_PROXY: proxyUrl,
    },
  });

  assert.strictEqual(result.status, 0, result.stderr.toString());
  proxy.close();
}));