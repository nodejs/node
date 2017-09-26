// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

// Verify that setTimeout callback verifications work correctly

{
  const server = http2.createServer();
  common.expectsError(
    () => server.setTimeout(10, 'test'),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
  common.expectsError(
    () => server.setTimeout(10, 1),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
}

{
  const server = http2.createSecureServer({});
  common.expectsError(
    () => server.setTimeout(10, 'test'),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
  common.expectsError(
    () => server.setTimeout(10, 1),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
}
