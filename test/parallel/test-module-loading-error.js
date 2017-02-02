'use strict';
require('../common');
const assert = require('assert');

const error_desc = {
  win32: ['%1 is not a valid Win32 application'],
  linux: ['file too short', 'Exec format error'],
  sunos: ['unknown file type', 'not an ELF file'],
  darwin: ['file too short']
};
const dlerror_msg = error_desc[process.platform];

assert.throws(
  () => { require('../fixtures/module-loading-error.node'); },
  (e) => {
    if (dlerror_msg && !dlerror_msg.some((msg) => e.message.includes(msg)))
      return false;
    if (e.name !== 'Error')
      return false;
    return true;
  }
);

assert.throws(
  require,
  /^AssertionError: missing path$/
);

assert.throws(
  () => { require({}); },
  /^AssertionError: path must be a string$/
);
