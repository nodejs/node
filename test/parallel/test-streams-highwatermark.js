'use strict';

require('../common');
const assert = require('assert');
const stream = require('stream');

const readable = stream.Readable({ highWaterMark: 2147483648 });
assert.strictEqual(readable._readableState.highWaterMark, 2147483648);

const writable = stream.Writable({ highWaterMark: 2147483648 });
assert.strictEqual(writable._writableState.highWaterMark, 2147483648);
