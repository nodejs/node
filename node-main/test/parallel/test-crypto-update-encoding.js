'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');

const zeros = Buffer.alloc;
const key = zeros(16);
const iv = zeros(16);

const cipher = () => crypto.createCipheriv('aes-128-cbc', key, iv);
const decipher = () => crypto.createDecipheriv('aes-128-cbc', key, iv);
const hash = () => crypto.createSign('sha256');
const hmac = () => crypto.createHmac('sha256', key);
const sign = () => crypto.createSign('sha256');
const verify = () => crypto.createVerify('sha256');

for (const f of [cipher, decipher, hash, hmac, sign, verify])
  for (const n of [15, 16])
    f().update(zeros(n), 'hex');  // Should ignore inputEncoding.
