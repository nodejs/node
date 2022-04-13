'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(require('dns/promises'), require('dns').promises);
