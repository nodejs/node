'use strict';
const common = require('../common');
global.process = {};  // Boom!
setImmediate(common.mustCall());
