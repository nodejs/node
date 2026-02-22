'use strict';

require('../common');
const assert = require('assert');
const { Readable } = require('stream');

const readable = new Readable({ read() {} });
readable.setEncoding('utf8');

readable.push('abc');
readable.push('defgh');
readable.push(null);

assert.strictEqual(readable.read(5), 'abcde');
assert.strictEqual(readable.read(3), 'fgh');
assert.strictEqual(readable.read(1), null);
