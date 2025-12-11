'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

assert.strictEqual(fs.readFileSync(fixtures.path('x.txt')).toString(), 'xyz\n');
