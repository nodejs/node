// Flags: --no-warnings
'use strict';
require('../common');
const spawn = require('node:child_process').spawn;
spawn(process.execPath,
      ['--no-warnings', '--test', '--test-reporter', 'tap', 'test/message/test_runner_output.js'],
      { stdio: 'inherit' });
