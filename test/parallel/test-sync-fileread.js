'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

assert.strictEqual(fixtures.readSync('x.txt').toString(), 'xyz\n');
