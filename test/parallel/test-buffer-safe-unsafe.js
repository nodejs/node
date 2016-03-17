'use strict';

require('../common');
const assert = require('assert');

const safe = Buffer.alloc(10);

function isZeroFilled(buf) {
  for (let n = 0; n < buf.length; n++)
    if (buf[n] > 0) return false;
  return true;
}

assert(isZeroFilled(safe));
