// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const constants = internalBinding('constants');

if (common.isLinux) {
  assert('O_NOATIME' in constants.fs);
  assert.strictEqual(constants.fs.O_NOATIME, 0x40000);
} else {
  assert(!('O_NOATIME' in constants.fs));
}
