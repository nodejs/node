'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const UUID_V4_REGEX =
  /[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[a-f0-9]{4}-[a-f0-9]{12}/;

// Test crypto.randomBytes() implementation
for (let i = 0; i < 100; i++) {
  const uuid = crypto.uuid();
  assert.ok(UUID_V4_REGEX.test(uuid));
}
