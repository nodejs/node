// The --expose-brotli flag is not set
'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => require('brotli'),
              /^Error: Cannot find module 'brotli'$/);
