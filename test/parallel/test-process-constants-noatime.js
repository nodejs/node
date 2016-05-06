'use strict';

require('../common');
const assert = require('assert');
const constants = process.binding('constants');

if (process.platform === 'linux') {
  assert(constants.hasOwnProperty('O_NOATIME'));
  assert.strictEqual(constants.O_NOATIME, 0x40000);
} else {
  assert(false === constants.hasOwnProperty('O_NOATIME'));
}
