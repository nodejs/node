'use strict';

require('../common');

const assert = require('assert');
const zlib = require('zlib');

assert.strictEqual(zlib.crc32('', -0), zlib.crc32('', 0));
