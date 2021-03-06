'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { inspect } = require('util');

// Verify that setTimeout callback verifications work correctly
const verifyCallbacks = (server) => {
  const testTimeout = 10;

  [true, 1, {}, [], null, 'test'].forEach((notFunction) => {
    assert.throws(
      () => server.setTimeout(testTimeout, notFunction),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_CALLBACK',
        message: 'Callback must be a function. ' +
                 `Received ${inspect(notFunction)}`
      }
    );
  });

  // No callback
  const returnedVal = server.setTimeout(testTimeout);
  assert.strictEqual(returnedVal.timeout, testTimeout);
};

// Test with server
{
  const server = http2.createServer();
  verifyCallbacks(server);
}

// Test with secure server
{
  const secureServer = http2.createSecureServer({});
  verifyCallbacks(secureServer);
}
