'use strict';
require('../common');
const child_process = require('child_process');
const fixtures = require('../common/fixtures');

child_process.fork(fixtures.path('empty.js')); // should not hang
