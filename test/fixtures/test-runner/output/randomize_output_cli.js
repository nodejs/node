'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

spawn(
  process.execPath,
  [
    '--no-warnings',
    '--test-reporter', 'spec',
    '--test-random-seed=12345',
    '--test',
    fixtures.path('test-runner/shards/*.cjs'),
  ],
  { stdio: 'inherit' },
);
