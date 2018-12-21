'use strict';

const common = require('../common');

if (common.isSunOS)
  common.skip(`Unsupported platform [${process.platform}]`);

if (!process.execArgv.includes('--title=foo'))
  common.relaunchWithFlags(['--title=foo']);

const assert = require('assert');

// Verifies that the --title=foo command line flag set the process
// title on startup.
assert.strictEqual(process.title, 'foo');
