'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const test = [
  {
    algorithm: 'aes128-wrap',
    key: 'b26f309fbe57e9b3bb6ae5ef31d54450',
    iv: '3fd838af4093d749',
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes128-wrap-pad',
    key: 'b26f309fbe57e9b3bb6ae5ef31d54450',
    iv: '3fd838af',
    text: '12345678123456781234567812345678123'
  },
  {
    algorithm: 'aes192-wrap',
    key: '40978085d68091f7dfca0d7dfc7a5ee76d2cc7f2f345a304',
    iv: '3fd838af4093d749',
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes192-wrap-pad',
    key: '40978085d68091f7dfca0d7dfc7a5ee76d2cc7f2f345a304',
    iv: '3fd838af',
    text: '12345678123456781234567812345678123'
  },
  {
    algorithm: 'aes256-wrap',
    key: '29c9eab5ed5ad44134a1437fe2e673b4d88a5b7c72e68454fea08721392b7323',
    iv: '3fd838af4093d749',
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes256-wrap-pad',
    key: '29c9eab5ed5ad44134a1437fe2e673b4d88a5b7c72e68454fea08721392b7323',
    iv: '3fd838af',
    text: '12345678123456781234567812345678123'
  },
];

test.forEach((data) => {
  const cipher = crypto.createCipheriv(
    data.algorithm,
    Buffer.from(data.key, 'hex'),
    Buffer.from(data.iv, 'hex'));
  const ciphertext = cipher.update(data.text, 'utf8');

  const decipher = crypto.createDecipheriv(
    data.algorithm,
    Buffer.from(data.key, 'hex'),
    Buffer.from(data.iv, 'hex'));
  const msg = decipher.update(ciphertext, 'buffer', 'utf8');

  assert.strictEqual(msg, data.text, `${data.algorithm} test case failed`);
});
