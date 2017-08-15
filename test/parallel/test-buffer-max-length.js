'use strict';

require('../common');
const assert = require('assert');
const buffer = require('buffer');

assert.strictEqual(buffer.kMaxLength,
                   buffer.constants.MAX_LENGTH);
assert.strictEqual(buffer.kStringMaxLength,
                   buffer.constants.MAX_STRING_LENGTH);
