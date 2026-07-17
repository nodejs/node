'use strict';
require('../../../common');
const fixtures = require('../../../common/fixtures');
const spawn = require('node:child_process').spawn;

const child = spawn(
  process.execPath,
  [
    '--no-warnings',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', 'lcov',
    fixtures.path('test-runner/output/output.js'),
  ],
  { stdio: 'inherit' },
);

child.on('error', (err) => {
  throw err;
});

child.on('exit', (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
    return;
  }

  process.exitCode = code ?? 1;
});
