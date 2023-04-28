// Flags: --title=foo
'use strict';

const common = require('../common');

if (common.isSunOS)
  common.skip(`Unsupported platform [${process.platform}]`);

if (common.isIBMi)
  common.skip('Unsupported platform IBMi');

const assert = require('assert');

// Verifies that the --title=foo command line flag set the process
// title on startup.
assert.strictEqual(process.title, 'foo');
