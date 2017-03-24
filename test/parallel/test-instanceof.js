'use strict';

const common = require('../common');
const assert = require('assert');


// Regression test for instanceof, see
// https://github.com/nodejs/node/issues/7592
const F = common.noop;
F.prototype = {};
assert(Object.create(F.prototype) instanceof F);
