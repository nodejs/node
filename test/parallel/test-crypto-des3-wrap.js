'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const ciphers = crypto.getCiphers();
if (!ciphers.includes('des3-wrap'))
  common.skip('des3-wrap cipher is not available');

// Test case for des-ede3 wrap/unwrap. des3-wrap needs extra 2x blocksize
// then plaintext to store ciphertext.
const test = {
  key: Buffer.from('3c08e25be22352910671cfe4ba3652b1220a8a7769b490ba', 'hex'),
  iv: Buffer.alloc(0),
  plaintext: '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBG' +
    'WWELweCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZU' +
    'JjAfaFg**'
};

const cipher = crypto.createCipheriv('des3-wrap', test.key, test.iv);
const ciphertext = cipher.update(test.plaintext, 'utf8');

const decipher = crypto.createDecipheriv('des3-wrap', test.key, test.iv);
const msg = decipher.update(ciphertext, 'buffer', 'utf8');

assert.strictEqual(msg, test.plaintext);
