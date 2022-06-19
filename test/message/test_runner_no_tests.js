// Flags: --no-warnings
'use strict';
require('../common');
const test = require('node:test');

// No TAP output should be generated.
console.log(test.name);
