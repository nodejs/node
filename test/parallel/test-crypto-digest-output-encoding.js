'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

for (const create of [
  () => crypto.createHash('sha256'),
  () => crypto.createHmac('sha256', 'key'),
]) {
  const expected = create().update('test').digest();
  for (const encoding of ['bad', { toString: () => 'bad' }]) {
    const instance = create().update('test');
    assert.throws(() => instance.digest(encoding), {
      code: 'ERR_UNKNOWN_ENCODING',
      message: 'Unknown encoding: bad',
    });
    // An invalid encoding must not finalize the operation.
    assert.deepStrictEqual(instance.digest(), expected);
  }
  for (const encoding of [undefined, null, '', false, 0, 'buffer', 'BUFFER']) {
    assert.deepStrictEqual(create().update('test').digest(encoding), expected);
  }
  for (const encoding of ['hex', 'HEX', 'base64', 'base64url', 'latin1', 'utf-8']) {
    assert.strictEqual(create().update('test').digest(encoding),
                       expected.toString(encoding));
  }
  assert.strictEqual(
    create().update('test').digest({ toString: () => 'hex' }),
    expected.toString('hex'));
}
