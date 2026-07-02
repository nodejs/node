'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test checks if error is thrown in case of wrong encoding provided into cipher.

const assert = require('assert');
const { createCipheriv, randomBytes } = require('crypto');

const createCipher = () => {
  return createCipheriv('aes-256-cbc', randomBytes(32), randomBytes(16));
};

{
  const cipher = createCipher();
  cipher.update('test', 'utf-8', 'utf-8');

  assert.throws(
    () => cipher.update('666f6f', 'hex', 'hex'),
    { message: /Cannot change encoding/ }
  );
}

{
  const cipher = createCipher();
  cipher.update('test', 'utf-8', 'utf-8');

  assert.throws(
    () => cipher.final('hex'),
    { message: /Cannot change encoding/ }
  );
}

{
  const cipher = createCipher();
  cipher.update('test', 'utf-8', 'utf-8');

  assert.throws(
    () => cipher.final('bad2'),
    { message: /^Unknown encoding: bad2$/, code: 'ERR_UNKNOWN_ENCODING' }
  );
}

{
  const cipher = createCipher();

  assert.throws(
    () => cipher.update('test', 'utf-8', 'bad3'),
    { message: /^Unknown encoding: bad3$/, code: 'ERR_UNKNOWN_ENCODING' }
  );
}

// Regression tests for https://github.com/nodejs/node/issues/45189:
// Unknown input encodings used to be silently accepted by Cipher/Decipher
// `update`, producing incorrect (and silently non-deterministic) output.
// They must now reject with ERR_UNKNOWN_ENCODING.

{
  const cipher = createCipher();

  assert.throws(
    () => cipher.update('test', 'bad', 'hex'),
    { message: /^Unknown encoding: bad$/, code: 'ERR_UNKNOWN_ENCODING' }
  );
}

{
  const { createDecipheriv } = require('crypto');
  const decipher = createDecipheriv(
    'aes-256-cbc', randomBytes(32), randomBytes(16));

  assert.throws(
    () => decipher.update('test', 'bad', 'hex'),
    { message: /^Unknown encoding: bad$/, code: 'ERR_UNKNOWN_ENCODING' }
  );
}

// A buffer-like data argument should not trigger encoding validation,
// because the input encoding is ignored when data is not a string.
{
  const cipher = createCipher();
  // Should not throw.
  cipher.update(Buffer.from('test'), 'bad-but-ignored', 'hex');
}

// Valid input encodings must continue to work.
{
  const cipher = createCipher();
  let result = cipher.update('test', 'utf-8', 'hex');
  result += cipher.final('hex');
  assert.strictEqual(typeof result, 'string');
}

// Omitting the input encoding (undefined / null) is allowed; the
// underlying binding falls back to its default behavior.
{
  const cipher = createCipher();
  // Should not throw.
  cipher.update(Buffer.from('test'));
}
