'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { execFileSync } = require('child_process');

const cjsModuleWrapTest = fixtures.path('cjs-module-wrap.js');
const node = process.execPath;

execFileSync(node, [cjsModuleWrapTest], { stdio: 'pipe' });
