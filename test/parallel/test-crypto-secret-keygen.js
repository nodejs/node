'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  generateKey,
  generateKeySync
} = require('crypto');

[1, true, [], {}, Infinity, null, undefined].forEach((i) => {
  assert.throws(() => generateKey(i, 1, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "type" argument must be /
  });
  assert.throws(() => generateKeySync(i, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "type" argument must be /
  });
});

['', true, [], null, undefined].forEach((i) => {
  assert.throws(() => generateKey('aes', i, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "options" argument must be /
  });
  assert.throws(() => generateKeySync('aes', i), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "options" argument must be /
  });
});

['', true, {}, [], null, undefined].forEach((length) => {
  assert.throws(() => generateKey('hmac', { length }, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "options\.length" property must be /
  });
  assert.throws(() => generateKeySync('hmac', { length }), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "options\.length" property must be /
  });
});

assert.throws(() => generateKey('aes', { length: 256 }), {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => generateKey('hmac', { length: -1 }, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => generateKey('hmac', { length: 4 }, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => generateKey('hmac', { length: 7 }, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(
  () => generateKey('hmac', { length: 2 ** 31 }, common.mustNotCall()), {
    code: 'ERR_OUT_OF_RANGE'
  });

assert.throws(() => generateKeySync('hmac', { length: -1 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => generateKeySync('hmac', { length: 4 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => generateKeySync('hmac', { length: 7 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(
  () => generateKeySync('hmac', { length: 2 ** 31 }), {
    code: 'ERR_OUT_OF_RANGE'
  });

assert.throws(() => generateKeySync('aes', { length: 123 }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.length' must be one of: 128, 192, 256/
});

{
  const key = generateKeySync('aes', { length: 128 });
  assert(key);
  const keybuf = key.export();
  assert.strictEqual(keybuf.byteLength, 128 / 8);

  generateKey('aes', { length: 128 }, common.mustSucceed((key) => {
    assert(key);
    const keybuf = key.export();
    assert.strictEqual(keybuf.byteLength, 128 / 8);
  }));
}

{
  const key = generateKeySync('aes', { length: 256 });
  assert(key);
  const keybuf = key.export();
  assert.strictEqual(keybuf.byteLength, 256 / 8);

  generateKey('aes', { length: 256 }, common.mustSucceed((key) => {
    assert(key);
    const keybuf = key.export();
    assert.strictEqual(keybuf.byteLength, 256 / 8);
  }));
}

{
  const key = generateKeySync('hmac', { length: 123 });
  assert(key);
  const keybuf = key.export();
  assert.strictEqual(keybuf.byteLength, Math.floor(123 / 8));

  generateKey('hmac', { length: 123 }, common.mustSucceed((key) => {
    assert(key);
    const keybuf = key.export();
    assert.strictEqual(keybuf.byteLength, Math.floor(123 / 8));
  }));
}

assert.throws(
  () => generateKey('unknown', { length: 123 }, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /The argument 'type' must be a supported key type/
  });

assert.throws(() => generateKeySync('unknown', { length: 123 }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'type' must be a supported key type/
});
