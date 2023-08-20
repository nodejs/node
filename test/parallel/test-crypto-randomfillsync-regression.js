'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { randomFillSync } = require('crypto');
const { notStrictEqual } = require('assert');

const ab = new ArrayBuffer(20);
const buf = Buffer.from(ab, 10);

const before = buf.toString('hex');

randomFillSync(buf);

const after = buf.toString('hex');

notStrictEqual(before, after);
