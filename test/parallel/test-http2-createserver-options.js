'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

// Error if invalid options are passed to createServer.
const invalidOptions = [1, true, 'test', null, Symbol('test')];
invalidOptions.forEach((invalidOption) => {
  assert.throws(
    () => http2.createServer(invalidOption),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type object.' +
               common.invalidArgTypeHelper(invalidOption)
    }
  );
});

// Error if invalid options.settings are passed to createServer.
invalidOptions.forEach((invalidSettingsOption) => {
  assert.throws(
    () => http2.createServer({ settings: invalidSettingsOption }),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options.settings" property must be of type object.' +
               common.invalidArgTypeHelper(invalidSettingsOption)
    }
  );
});

// Test that http2.createServer validates input options.
Object.entries({
  maxSessionInvalidFrames: [
    {
      val: -1,
      err: {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      },
    },
    {
      val: Number.NEGATIVE_INFINITY,
      err: {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      },
    },
  ],
  maxSessionRejectedStreams: [
    {
      val: -1,
      err: {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      },
    },
    {
      val: Number.NEGATIVE_INFINITY,
      err: {
        name: 'RangeError',
        code: 'ERR_OUT_OF_RANGE',
      },
    },
  ]
}).forEach(([opt, tests]) => {
  tests.forEach(({ val, err }) => {
    assert.throws(
      () => http2.createServer({ [opt]: val }),
      err
    );
  });
});
