'use strict';
var common = require('../common');
var child_process = require('child_process');

child_process.fork(common.fixturesDir + '/empty.js'); // should not hang
