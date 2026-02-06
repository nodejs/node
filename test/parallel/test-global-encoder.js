'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

assert.strictEqual(TextDecoder, util.TextDecoder);
assert.strictEqual(TextEncoder, util.TextEncoder);
