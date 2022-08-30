'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const assert = require('assert');
const dc = require('diagnostics_channel');
const crypto = require('crypto');

function testCreateSign() {
  const createChannel = 'crypto.sign.new';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  crypto.createSign('sha256');
  assert.strictEqual(seen, 'sha256');
  dc.unsubscribe(createChannel, createHandler);
}

function testCreateVerify() {
  const createChannel = 'crypto.verify.new';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  crypto.createVerify('sha256');
  assert.strictEqual(seen, 'sha256');
  dc.unsubscribe(createChannel, createHandler);
}

function testSign() {
  const createChannel = 'crypto.sign.new';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  const { privateKey } = crypto.generateKeyPairSync('rsa', {
    modulusLength: 2048,
  });
  crypto.sign('sha256', 'valuetosign', privateKey);
  assert.strictEqual(seen, 'sha256');
  dc.unsubscribe(createChannel, createHandler);
}

function testVerify() {
  const { privateKey, publicKey } = crypto.generateKeyPairSync('rsa', {
    modulusLength: 2048,
  });
  const data = 'valuetosign';
  const signature = crypto.sign('sha256', data, privateKey);

  const createChannel = 'crypto.verify.new';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  crypto.verify('sha256', data, publicKey, signature);
  assert.strictEqual(seen, 'sha256');
  dc.unsubscribe(createChannel, createHandler);
}

testCreateSign();
testCreateVerify();
testSign();
testVerify();
