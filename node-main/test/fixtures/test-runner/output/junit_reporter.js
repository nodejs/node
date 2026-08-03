'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(process.execPath,
      [
        '--no-warnings', '--test-reporter', 'junit',
        fixtures.path('test-runner/output/output.js'),
      ],
      { stdio: 'inherit' });
