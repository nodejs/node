'use strict';
const common = require('../../../common');
const { once } = require('node:events');
const { spawn } = require('node:child_process');
const fixtures = require('../../../common/fixtures');

(async function run() {
  const test = fixtures.path('test-runner/output/arbitrary-output-colored-1.js');
  await once(spawn(process.execPath, ['--test', test], { stdio: 'inherit', env: { FORCE_COLOR: 1 } }), 'exit');
  await once(spawn(process.execPath, ['--test', '--test-reporter', 'tap', test], { stdio: 'inherit', env: { FORCE_COLOR: 1 }  }), 'exit');
})().then(common.mustCall());
