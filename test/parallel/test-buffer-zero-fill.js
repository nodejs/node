'use strict';

require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;

const buf1 = Buffer(100);
const buf2 = new Buffer(100);

let n = 0;

for (n = 0; n < buf1.length; n++)
  assert.strictEqual(buf1[n], 0);

for (n = 0; n < buf2.length; n++)
  assert.strictEqual(buf2[n], 0);
