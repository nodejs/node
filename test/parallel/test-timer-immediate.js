'use strict';
const common = require('../common');
global.process = {};  // Boom!
common.allowGlobals(global.process);
setImmediate(common.mustCall());
