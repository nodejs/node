// This tests that proxy URLs with invalid credentials (containing \r or \n characters)
// are rejected with an appropriate error.
import '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';

const testCases = [
  {
    name: 'carriage return in username',
    proxyUrl: 'http://user\r:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'newline in username',
    proxyUrl: 'http://user\n:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'carriage return in password',
    proxyUrl: 'http://user:pass\r@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'newline in password',
    proxyUrl: 'http://user:pass\n@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'CRLF injection attempt in username',
    proxyUrl: 'http://user\r\nHost: example.com:pass@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
  {
    name: 'CRLF injection attempt in password',
    proxyUrl: 'http://user:pass\r\nHost: example.com@proxy.example.com:8080',
    expectedError: { code: 'ERR_PROXY_INVALID_CONFIG', message: /Invalid proxy URL/ },
  },
];

for (const testCase of testCases) {
  // Test that creating an agent with invalid proxy credentials throws an error
  assert.throws(() => {
    new http.Agent({
      proxyEnv: {
        HTTP_PROXY: testCase.proxyUrl,
      },
    });
  }, testCase.expectedError);
}
