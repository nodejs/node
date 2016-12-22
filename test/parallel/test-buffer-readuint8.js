'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt8()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readUInt8(2);

assert.strictEqual(result, 168);

assert.throws(
   () => { buf.readUInt8(5); }, RangeError);

assert.doesNotThrow(
   () => { assert.strictEqual(buf.readUInt8(5, 0, false), undefined); },
   'readUIntBE() should not throw if noAssert is true'
);
