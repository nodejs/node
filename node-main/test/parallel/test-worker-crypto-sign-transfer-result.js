'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { Worker } = require('worker_threads');
const fixturesPath = require.resolve('../common/fixtures');

// Test that transferring the result of e.g. crypto.sign() from Worker to parent
// thread does not crash

const w = new Worker(`
const { parentPort } = require('worker_threads');
const crypto = require('crypto');
const assert = require('assert');
const fixtures = require(${JSON.stringify(fixturesPath)});

const keyPem = fixtures.readKey('rsa_private.pem');

const buf = crypto.sign('sha256', Buffer.from('hello'), keyPem);
assert.notStrictEqual(buf.byteLength, 0);
parentPort.postMessage(buf, [buf.buffer]);
assert.strictEqual(buf.byteLength, 0);
`, { eval: true });

w.on('message', common.mustCall((buf) => {
  assert.notStrictEqual(buf.byteLength, 0);
}));
w.on('exit', common.mustCall());
