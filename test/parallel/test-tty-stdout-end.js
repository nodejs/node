'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
require('../common');
const assert = require('assert');

assert.throws(() => process.stdout.end(), /process.stdout cannot be closed./);
