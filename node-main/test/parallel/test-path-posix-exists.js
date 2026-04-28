'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(require('path/posix'), require('path').posix);
