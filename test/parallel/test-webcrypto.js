'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { strictEqual } = require('assert');
const {
  webcrypto,
  subtle,
  getRandomValues,
} = require('crypto');

strictEqual(subtle, webcrypto.subtle);
strictEqual(getRandomValues, webcrypto.getRandomValues);
