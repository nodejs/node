'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { getRandomValues } = require('crypto').webcrypto;

assert.throws(() => getRandomValues(new Uint8Array()), { code: 'ERR_INVALID_THIS' });
