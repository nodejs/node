'use strict';

require('../common');

const fixtures = require('../common/fixtures');
const { Worker } = require('node:worker_threads');

process.env.NODE_OPTIONS ??= '';
process.env.NODE_OPTIONS += ` --require ${JSON.stringify(fixtures.path('define-global.js'))}`;

new Worker(fixtures.path('assert-global.js'));
