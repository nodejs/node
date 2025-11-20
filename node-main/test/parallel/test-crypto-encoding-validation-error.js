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
