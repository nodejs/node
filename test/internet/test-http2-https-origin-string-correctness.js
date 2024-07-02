'use strict';

// Ref: https://github.com/nodejs/node/issues/39919

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

function _verifyOriginSet(session, originString) {
  session.once('remoteSettings', () => {
    assert.strictEqual(typeof session.originSet, 'object');
    assert.strictEqual(session.originSet.length, 1);
    assert.strictEqual(session.originSet[0], originString);
    session.close();
  });
  session.once('error', (error) => {
    assert.strictEqual(error.code, 'ECONNREFUSED');
    session.close();
  });
}

function withServerName() {
  const session = http2.connect('https://1.1.1.1', { servername: 'cloudflare-dns.com' });
  _verifyOriginSet(session, 'https://cloudflare-dns.com');
}

function withEmptyServerName() {
  const session = http2.connect('https://1.1.1.1', { servername: '' });
  _verifyOriginSet(session, 'https://1.1.1.1');
}

function withoutServerName() {
  const session = http2.connect('https://1.1.1.1');
  _verifyOriginSet(session, 'https://1.1.1.1');
}

withServerName();
withEmptyServerName();
withoutServerName();
