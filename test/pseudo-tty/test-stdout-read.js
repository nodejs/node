'use strict';
const common = require('../common');
process.stderr.on('data', common.mustCall(console.log));
