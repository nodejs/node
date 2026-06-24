'use strict';

const os = require('node:os');

const isMacOS15X64 =
  os.platform() === 'darwin' &&
  process.arch === 'x64' &&
  os.release().startsWith('24.');

module.exports = {
  ...(isMacOS15X64 ? {
    'basic.any.js': {
      fail: {
        note: 'Flaky on macos15-x64: Date.now() can shift relative to monotonic performance.now()',
        flaky: [
          'High resolution time has approximately the right relative magnitude',
        ],
      },
    },
  } : {}),

  'idlharness.any.js': {
    fail: {
      expected: [
        'Window interface: attribute performance',
      ],
    },
  },

  'idlharness-shadowrealm.window.js': {
    skip: 'ShadowRealm support is not enabled',
  },

  'window-worker-timeOrigin.window.js': {
    skip: 'depends on URL.createObjectURL(blob)',
  },
};
