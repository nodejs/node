'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

// Error if invalid options are passed to createServer
const invalidOptions = [1, true, 'test', null, Symbol('test')];
invalidOptions.forEach((invalidOption) => {
  assert.throws(
    () => http2.createServer(invalidOption),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type Object. Received ' +
               `type ${typeof invalidOption}`
    }
  );
});

// Error if invalid options.settings are passed to createServer
invalidOptions.forEach((invalidSettingsOption) => {
  assert.throws(
    () => http2.createServer({ settings: invalidSettingsOption }),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.settings" property must be of type Object. ' +
               `Received type ${typeof invalidSettingsOption}`
    }
  );
});
