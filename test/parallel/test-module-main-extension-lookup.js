'use strict';
require('../common');
const { execFileSync } = require('child_process');

const node = process.argv[0];

execFileSync(node, ['--experimental-modules', 'test/es-module/test-esm-ok']);
