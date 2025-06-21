'use strict';

const common = require('../common');
const assert = require('assert');
const constants = require('fs').constants;

if (common.isLinux) {
  assert('O_NOATIME' in constants);
  assert.strictEqual(constants.O_NOATIME, 0x40000);
} else {
  assert(!('O_NOATIME' in constants));
}
