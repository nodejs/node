'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const crypto = require('crypto');
const tls = require('tls');

const keys = crypto.randomBytes(48);
const otherKeys = crypto.randomBytes(48);

const context = tls.createSecureContext({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ticketKeys: keys,
  sessionTimeout: 1,
});

assert.deepStrictEqual(context.getTicketKeys(), keys);
context.setTicketKeys(otherKeys);
assert.deepStrictEqual(context.getTicketKeys(), otherKeys);
setTimeout(() => assert.deepStrictEqual(context.getTicketKeys(), otherKeys), 5000);
