'use strict';
const common = require('../common');
common.globalCheck = false;
// eslint-disable-next-line no-global-assign
process = {};  // Boom!
setImmediate(common.mustCall());
