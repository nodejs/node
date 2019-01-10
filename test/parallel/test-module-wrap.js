'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { execFileSync } = require('child_process');

const cjsModuleWrapTest = fixtures.path('cjs-module-wrap.js');
const node = process.argv[0];

execFileSync(node, [cjsModuleWrapTest], { stdio: 'pipe' });
