// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

// Error if options are not passed to createSecureServer

common.expectsError(
  () => http2.createSecureServer(),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => http2.createSecureServer(() => {}),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => http2.createSecureServer(1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => http2.createSecureServer('test'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

common.expectsError(
  () => http2.createSecureServer(null),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });
