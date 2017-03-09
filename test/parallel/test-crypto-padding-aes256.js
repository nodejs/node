'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

function aes256(decipherFinal) {
  const iv = Buffer.from('00000000000000000000000000000000', 'hex');
  const key = Buffer.from('0123456789abcdef0123456789abcdef' +
                         '0123456789abcdef0123456789abcdef', 'hex');

  function encrypt(val, pad) {
    const c = crypto.createCipheriv('aes256', key, iv);
    c.setAutoPadding(pad);
    return c.update(val, 'utf8', 'latin1') + c.final('latin1');
  }

  function decrypt(val, pad) {
    const c = crypto.createDecipheriv('aes256', key, iv);
    c.setAutoPadding(pad);
    return c.update(val, 'latin1', 'utf8') + c[decipherFinal]('utf8');
  }

  // echo 0123456789abcdef0123456789abcdef \
  // | openssl enc -e -aes256 -nopad -K <key> -iv <iv> \
  // | openssl enc -d -aes256 -nopad -K <key> -iv <iv>
  let plaintext = '0123456789abcdef0123456789abcdef'; // multiple of block size
  let encrypted = encrypt(plaintext, false);
  let decrypted = decrypt(encrypted, false);
  assert.strictEqual(decrypted, plaintext);

  // echo 0123456789abcdef0123456789abcde \
  // | openssl enc -e -aes256 -K <key> -iv <iv> \
  // | openssl enc -d -aes256 -K <key> -iv <iv>
  plaintext = '0123456789abcdef0123456789abcde'; // not a multiple
  encrypted = encrypt(plaintext, true);
  decrypted = decrypt(encrypted, true);
  assert.strictEqual(decrypted, plaintext);
}

aes256('final');
aes256('finaltol');
