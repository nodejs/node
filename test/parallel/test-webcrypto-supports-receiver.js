'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { supports } = SubtleCrypto;

assert.strictEqual(supports('digest', 'SHA-256'), true);
for (const receiver of [undefined, null, {}, globalThis.crypto.subtle]) {
  assert.strictEqual(supports.call(receiver, 'digest', 'SHA-256'), true);
  assert.strictEqual(supports.call(receiver, 'digest', 'unknown'), false);
}
