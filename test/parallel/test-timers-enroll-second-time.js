'use strict';

const common = require('../common');

const assert = require('assert');
const timers = require('timers');

let didCall = false;
const enrollObj = {
  _onTimeout: common.mustCall(() => assert(didCall)),
};

timers.enroll(enrollObj, 1);
timers.enroll(enrollObj, 10);
timers.active(enrollObj);
setTimeout(common.mustCall(() => { didCall = true; }), 1);
