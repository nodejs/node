'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(process.execPath,
      ['--no-warnings', '--test', '--test-reporter', 'tap', fixtures.path('test-runner/output/output.js'), fixtures.path('test-runner/output/single.js')],
      { stdio: 'inherit' });
