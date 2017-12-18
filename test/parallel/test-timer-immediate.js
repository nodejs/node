'use strict';
const common = require('../common');
common.globalCheck = false;
process = {};  // Boom!
setImmediate(common.mustCall());
