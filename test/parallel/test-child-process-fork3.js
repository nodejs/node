'use strict';
const common = require('../common');
const child_process = require('child_process');

child_process.fork(common.fixturesDir + '/empty.js'); // should not hang
