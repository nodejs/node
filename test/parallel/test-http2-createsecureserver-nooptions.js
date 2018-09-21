'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

// Error if options are not passed to createSecureServer
const invalidOptions = [() => {}, 1, 'test', null];
invalidOptions.forEach((invalidOption) => {
  assert.throws(
    () => http2.createSecureServer(invalidOption),
    {
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type Object. Received ' +
               `type ${typeof invalidOption}`
    }
  );
});
