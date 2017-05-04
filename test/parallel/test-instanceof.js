'use strict';

require('../common');
const assert = require('assert');


// Regression test for instanceof, see
// https://github.com/nodejs/node/issues/7592
const F = () => {};
F.prototype = {};
assert(Object.create(F.prototype) instanceof F);
