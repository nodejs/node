'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { getEventListeners, once } = require('events');

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
const server = tls.createServer(serverOptions);
server.listen(0, common.mustCall(async () => {
  const port = server.address().port;
  const host = 'localhost';
  const connectOptions = (signal) => ({
    port,
    host,
    signal,
    rejectUnauthorized: false,
  });

  function assertAbort(socket, testName) {
    return assert.rejects(() => once(socket, 'close'), {
      name: 'AbortError',
    }, `close ${testName} should have thrown`);
  }

  async function postAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = tls.connect(connectOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    await assertAbort(socket, 'postAbort');
  }

  async function preAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const socket = tls.connect(connectOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    await assertAbort(socket, 'preAbort');
  }

  async function tickAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = tls.connect(connectOptions(signal));
    setImmediate(() => ac.abort());
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    await assertAbort(socket, 'tickAbort');
  }

  async function testConstructor() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const socket = new tls.TLSSocket(undefined, connectOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    await assertAbort(socket, 'testConstructor');
  }

  async function testConstructorPost() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = new tls.TLSSocket(undefined, connectOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    await assertAbort(socket, 'testConstructorPost');
  }

  async function testConstructorPostTick() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = new tls.TLSSocket(undefined, connectOptions(signal));
    setImmediate(() => ac.abort());
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    await assertAbort(socket, 'testConstructorPostTick');
  }

  await postAbort();
  await preAbort();
  await tickAbort();
  await testConstructor();
  await testConstructorPost();
  await testConstructorPostTick();

  server.close(common.mustCall());
}));
