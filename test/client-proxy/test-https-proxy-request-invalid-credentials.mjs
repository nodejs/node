// This tests that HTTPS proxy URLs with invalid credentials (containing \r or \n characters)
// are rejected with an appropriate error.
import * as common from '../common/index.mjs';
import assert from 'node:assert';

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const testCases = [
  {
    name: 'carriage return in username (HTTPS)',
    proxyUrl: 'https://user\r:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'newline in username (HTTPS)',
    proxyUrl: 'https://user\n:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'carriage return in password (HTTPS)',
    proxyUrl: 'https://user:pass\r@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'newline in password (HTTPS)',
    proxyUrl: 'https://user:pass\n@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'CRLF injection attempt in username (HTTPS)',
    proxyUrl: 'https://user\r\nHost: example.com:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'CRLF injection attempt in password (HTTPS)',
    proxyUrl: 'https://user:pass\r\nHost: example.com@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
];

for (const testCase of testCases) {
  // Test that creating an agent with invalid proxy credentials throws an error
  assert.throws(() => {
    new https.Agent({
      proxyEnv: {
        HTTPS_PROXY: testCase.proxyUrl,
      },
    });
  }, testCase.expectedError);
}
