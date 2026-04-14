'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(
  process.execPath,
  [
    '--no-warnings',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', 'lcov',
    fixtures.path('test-runner/output/brda_ignore_output.js'),
  ],
  { stdio: 'inherit' },
);
