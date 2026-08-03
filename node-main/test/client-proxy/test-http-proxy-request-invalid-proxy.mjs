// This tests that constructing agents with invalid proxy URLs throws ERR_PROXY_INVALID_CONFIG.
import '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';

const testCases = [
  {
    name: 'invalid IPv4 address',
    proxyUrl: 'http://256.256.256.256:8080',
  },
  {
    name: 'invalid IPv6 address',
    proxyUrl: 'http://::1:8080',
  },
  {
    name: 'missing host',
    proxyUrl: 'http://:8080',
  },
  {
    name: 'non-numeric port',
    proxyUrl: 'http://proxy.example.com:port',
  },
];

for (const testCase of testCases) {
  assert.throws(() => {
    new http.Agent({
      proxyEnv: {
        HTTP_PROXY: testCase.proxyUrl,
      },
    });
  }, {
    code: 'ERR_PROXY_INVALID_CONFIG',
    message: `Invalid proxy URL: ${testCase.proxyUrl}`,
  });
}
