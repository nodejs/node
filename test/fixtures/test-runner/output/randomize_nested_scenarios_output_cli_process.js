'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(
  process.execPath,
  [
    '--no-warnings',
    '--test-reporter', 'spec',
    '--test-random-seed=1',
    '--test-isolation=process',
    '--test',
    fixtures.path('test-runner/randomize/internal-order-nested-scenarios.cjs'),
  ],
  { stdio: 'inherit' },
);
