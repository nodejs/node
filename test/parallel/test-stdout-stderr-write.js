'use strict';

require('../common');
const assert = require('assert');

// https://github.com/nodejs/node/pull/39246
assert.strictEqual(process.stderr.write('asd'), true);
assert.strictEqual(process.stdout.write('asd'), true);
