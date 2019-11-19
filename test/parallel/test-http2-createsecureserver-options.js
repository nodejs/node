'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

// Error if invalid options are passed to createSecureServer
const invalidOptions = [() => {}, 1, 'test', null, Symbol('test')];
invalidOptions.forEach((invalidOption) => {
  assert.throws(
    () => http2.createSecureServer(invalidOption),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type Object. Received ' +
               `type ${typeof invalidOption}`
    }
  );
});

// Error if invalid options.settings are passed to createSecureServer
invalidOptions.forEach((invalidSettingsOption) => {
  assert.throws(
    () => http2.createSecureServer({ settings: invalidSettingsOption }),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.settings" property must be of type Object. ' +
               `Received type ${typeof invalidSettingsOption}`
    }
  );
});
