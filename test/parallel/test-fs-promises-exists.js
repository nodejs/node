'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(require('fs/promises'), require('fs').promises);
