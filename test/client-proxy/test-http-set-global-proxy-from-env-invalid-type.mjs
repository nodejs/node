// Tests that http.setGlobalProxyFromEnv() validates that proxyEnv is an object.

import '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';

for (const invalidInput of [42, 'string', null, [], true]) {
  assert.throws(
    () => http.setGlobalProxyFromEnv(invalidInput),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    },
  );
}
