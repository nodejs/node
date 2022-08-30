'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const assert = require('assert');
const dc = require('diagnostics_channel');
const crypto = require('crypto');

function testCipherDecipher() {
  const methods = [
    {
      channel: 'crypto.cipher.new',
      method: 'createCipher'
    },
    {
      channel: 'crypto.decipher.new',
      method: 'createDecipher'
    },
  ];
  methods.forEach(({ channel, method }) => {
    let seen;
    const handler = common.mustCall(({ algorithm }) => {
      seen = algorithm;
    });
    dc.subscribe(channel, handler);
    const secret = 'SECRET';
    const algorithm = 'aes-256-cbc';
    crypto[method](algorithm, secret);
    assert.strictEqual(seen, algorithm);
    dc.unsubscribe(channel, handler);
  });
}

function testCipherivDecipheriv() {
  const methods = [
    {
      channel: 'crypto.cipheriv.new',
      method: 'createCipheriv'
    },
    {
      channel: 'crypto.decipheriv.new',
      method: 'createDecipheriv'
    },
  ];
  methods.forEach(({ channel, method }) => {
    let seen;
    const handler = common.mustCall(({ algorithm }) => {
      seen = algorithm;
    });
    dc.subscribe(channel, handler);
    const secret = crypto.randomBytes(32);
    const algorithm = 'aes-256-cbc';
    const iv = crypto.randomBytes(16);
    crypto[method](algorithm, secret, iv);
    assert.strictEqual(seen, algorithm);
    dc.unsubscribe(channel, handler);
  });
}

testCipherDecipher();
testCipherivDecipheriv();
