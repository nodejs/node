'use strict';

const common = require('../common');

const assert = require('assert');
const timers = require('timers');

const enrollObj = {
  _onTimeout: common.mustCall(),
};

timers.enroll(enrollObj, 1);
assert.strictEqual(enrollObj._idleTimeout, 1);
timers.enroll(enrollObj, 10);
assert.strictEqual(enrollObj._idleTimeout, 10);
timers.active(enrollObj);
