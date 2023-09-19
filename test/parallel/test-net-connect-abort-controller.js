'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
const server = net.createServer();
const { getEventListeners, once } = require('events');

const liveConnections = new Set();

server.listen(0, common.mustCall(async () => {
  const port = server.address().port;
  const host = 'localhost';
  const socketOptions = (signal) => ({ port, host, signal });
  server.on('connection', (connection) => {
    liveConnections.add(connection);
    connection.on('close', () => {
      liveConnections.delete(connection);
    });
  });

  const assertAbort = async (socket, testName) => {
    try {
      await once(socket, 'close');
      assert.fail(`close ${testName} should have thrown`);
    } catch (err) {
      assert.strictEqual(err.name, 'AbortError');
    }
  };

  async function postAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = net.connect(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    await assertAbort(socket, 'postAbort');
  }

  async function preAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const socket = net.connect(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    await assertAbort(socket, 'preAbort');
  }

  async function tickAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    setImmediate(() => ac.abort());
    const socket = net.connect(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    await assertAbort(socket, 'tickAbort');
  }

  async function testConstructor() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const socket = new net.Socket(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    await assertAbort(socket, 'testConstructor');
  }

  async function testConstructorPost() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = new net.Socket(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    await assertAbort(socket, 'testConstructorPost');
  }

  async function testConstructorPostTick() {
    const ac = new AbortController();
    const { signal } = ac;
    const socket = new net.Socket(socketOptions(signal));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    setImmediate(() => ac.abort());
    await assertAbort(socket, 'testConstructorPostTick');
  }

  await postAbort();
  await preAbort();
  await tickAbort();
  await testConstructor();
  await testConstructorPost();
  await testConstructorPostTick();

  // Killing the net.socket without connecting hangs the server.
  for (const connection of liveConnections) {
    connection.destroy();
  }
  server.close(common.mustCall());
}));
