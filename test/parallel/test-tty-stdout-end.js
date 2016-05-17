'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
const common = require('../common');

common.throws(() => process.stdout.end(), {code: 'STDOUTCLOSE', type: Error});
