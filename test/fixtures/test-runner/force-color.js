const {WriteStream} = require('tty');
const assert = require('assert');

assert.strictEqual(process.env.FORCE_COLOR, '3');
assert.strictEqual(WriteStream(0).getColorDepth(), 24);
