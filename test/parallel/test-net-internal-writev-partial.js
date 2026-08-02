// Flags: --expose-gc --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const asyncHooks = require('async_hooks');
const net = require('net');
const { kInternalWritev } = require('internal/streams/utils');

const prefix = 'BEGIN';
const suffix = 'END';
const payloadLength = 32 * 1024 * 1024;
const expectedLength = prefix.length + payloadLength + suffix.length;

let accepted;
let client;
let started = false;
let chunks;
let writeWrap;

const onWriteWrapInit = common.mustCall((resource) => {
  assert.strictEqual(writeWrap, undefined);
  // The native request is observable at async-init before C++ returns it to
  // JS. Its stable fields exist, but dispatch metadata is attached only after
  // the binding call completes.
  assert.strictEqual(resource.handle, null);
  assert.strictEqual(resource.oncomplete, null);
  assert.strictEqual(resource.callback, null);
  assert.strictEqual(resource.async, false);
  assert.strictEqual(resource.bytes, 0);
  assert.strictEqual(resource.buffer, null);
  assert.strictEqual(Object.hasOwn(resource, '_chunks'), false);
  writeWrap = resource;
});

const hook = asyncHooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type !== 'WRITEWRAP') {
      return;
    }
    onWriteWrapInit(resource);
  },
});
hook.enable();

let received = 0;
let first = Buffer.alloc(0);
let last = Buffer.alloc(0);

const timer = setTimeout(
  common.mustNotCall('timed out waiting for the partial write'),
  common.platformTimeout(10_000),
);

const server = net.createServer(common.mustCall((socket) => {
  accepted = socket;
  socket.on('data', (chunk) => {
    if (first.length < prefix.length) {
      const needed = prefix.length - first.length;
      first = Buffer.concat([first, chunk.subarray(0, needed)]);
    }
    if (chunk.length >= suffix.length) {
      last = chunk.subarray(chunk.length - suffix.length);
    } else {
      last = Buffer.concat([last, chunk]);
      if (last.length > suffix.length) {
        last = last.subarray(last.length - suffix.length);
      }
    }
    received += chunk.length;
  });
  socket.on('end', common.mustCall(() => {
    assert.strictEqual(received, expectedLength);
    assert.strictEqual(first.toString(), prefix);
    assert.strictEqual(last.toString(), suffix);
    clearTimeout(timer);
    hook.disable();
    server.close(common.mustCall());
  }));
  socket.pause();
  startWrite();
}));

function startWrite() {
  if (started || accepted === undefined || client === undefined ||
      client.connecting) {
    return;
  }
  started = true;

  chunks = [
    prefix, 'latin1',
    Buffer.alloc(payloadLength, 0x78), 'buffer',
    suffix, 'latin1',
  ];
  assert.strictEqual(client[kInternalWritev](chunks, common.mustCall(() => {
    client.end();
  })), false);

  // The payload is larger than the kernel send buffer, so uv_try_write()
  // must leave an asynchronous remainder and lazily create one WriteWrap.
  assert.notStrictEqual(writeWrap, undefined);
  assert.strictEqual(writeWrap.handle, client._handle);
  assert.strictEqual(typeof writeWrap.oncomplete, 'function');
  assert.strictEqual(typeof writeWrap.callback, 'function');
  assert.strictEqual(writeWrap.async, true);
  assert.strictEqual(writeWrap.bytes, expectedLength);
  assert.strictEqual(writeWrap.buffer, chunks);
  assert.strictEqual(Object.hasOwn(writeWrap, '_chunks'), false);

  chunks = null;
  global.gc();
  setImmediate(() => accepted.resume());
}

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  client = net.createConnection({
    host: common.localhostIPv4,
    port: server.address().port,
  }, common.mustCall(startWrite));
  client.on('error', common.mustNotCall());
}));
