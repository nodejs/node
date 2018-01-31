'use strict';
const common = require('../common');
common.globalCheck = false;
global.process = {};  // Boom!
setImmediate(common.mustCall());
