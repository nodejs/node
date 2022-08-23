'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const assert = require('assert');
const dc = require('diagnostics_channel');
const crypto = require('crypto');

function testHashAndCopy() {
  const createChannel = 'crypto.hash.new';
  const copyChannel = 'crypto.hash.copy';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  const hash = crypto.createHash('sha1');
  assert.strictEqual(seen, 'sha1');
  dc.unsubscribe(createChannel, createHandler);
  const copyHandler = common.mustCall();
  dc.subscribe(copyChannel, copyHandler);
  hash.copy();
  dc.unsubscribe(copyChannel, copyHandler);
}

function testHmac() {
  const createChannel = 'crypto.hmac.new';
  let seen;
  const createHandler = common.mustCall(({ algorithm }) => {
    seen = algorithm;
  });
  dc.subscribe(createChannel, createHandler);
  crypto.createHmac('sha256', 'SECRET');
  assert.strictEqual(seen, 'sha256');
  dc.unsubscribe(createChannel, createHandler);
}

testHashAndCopy();
testHmac();
