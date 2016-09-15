'use strict';
// Refs: https://github.com/nodejs/node/issues/8539
require('../common');

const file = {};
file.toString = () => { throw 'foo'; };
const child_process = require('child_process');
child_process.spawnSync(file);
