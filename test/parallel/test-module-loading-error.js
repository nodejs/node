'use strict';
const common = require('../common');
const assert = require('assert');

const errorMessagesByPlatform = {
  win32: ['is not a valid Win32 application'],
  linux: ['file too short', 'Exec format error'],
  sunos: ['unknown file type', 'not an ELF file'],
  darwin: ['file too short'],
  aix: ['Cannot load module',
        'Cannot run a file that does not have a valid format.']
};
const dlerror_msg = errorMessagesByPlatform[process.platform];

if (!dlerror_msg)
  common.skip('platform not supported.');

try {
  require('../fixtures/module-loading-error.node');
} catch (e) {
  assert.strictEqual(dlerror_msg.some((errMsgCase) => {
    return e.toString().indexOf(errMsgCase) !== -1;
  }), true);
}

try {
  require();
} catch (e) {
  assert.notStrictEqual(e.toString().indexOf('missing path'), -1);
}

try {
  require({});
} catch (e) {
  assert.notStrictEqual(e.toString().indexOf('path must be a string'), -1);
}
