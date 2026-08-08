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
    '--test-coverage-lines=99',
    '--test-reporter', 'dot',
    fixtures.path('test-runner/coverage.js'),
  ],
  { stdio: 'inherit' },
);
