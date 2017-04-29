'use strict';
require('../common');
const assert = require('assert');

assert.throws(() => {
  process.stdout.write('broken', 'buffer');
}, /^TypeError: Second argument must be a buffer$/);
