'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createDecipheriv, randomBytes } = require('crypto');

const key = randomBytes(32);
const iv = randomBytes(16);
for (let tagLength = 0; tagLength < 16; tagLength++) {
  const tag = randomBytes(tagLength);
  assert.throws(() => {
    createDecipheriv('aes-256-gcm', key, iv).setAuthTag(tag);
  }, {
    message: `Invalid authentication tag length: ${tagLength}`,
  });
}
