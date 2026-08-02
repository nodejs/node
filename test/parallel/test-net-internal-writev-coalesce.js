// Flags: --expose-gc --expose-internals --no-warnings

'use strict';

const common = require('../common');
const assert = require('assert');
const asyncHooks = require('async_hooks');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const { kInternalWritev } = require('internal/streams/utils');
const {
  WriteWrap,
  kLastWriteWasAsync,
  streamBaseState,
} = internalBinding('stream_wrap');

const direct = Buffer.alloc(32 * 1024 * 1024, 0x78);
let vector = [];
let expected = '';
for (let i = 0; i < 32; i++) {
  const value = String.fromCharCode(0x41 + (i % 26)).repeat(2048);
  vector.push(value, 'latin1');
  expected += value;
}
expected = Buffer.from(expected);

let accepted;
let client;
let started = false;
const writeWraps = [];

const hook = asyncHooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'WRITEWRAP') {
      writeWraps.push(resource);
    }
  },
});
hook.enable();

let received = 0;
let tail = Buffer.alloc(0);
const timer = setTimeout(
  common.mustNotCall('timed out waiting for the coalesced write'),
  common.platformTimeout(10_000),
);

const server = net.createServer(common.mustCall((socket) => {
  accepted = socket;
  socket.on('data', (chunk) => {
    received += chunk.length;
    if (chunk.length >= expected.length) {
      tail = chunk.subarray(-expected.length);
    } else {
      tail = Buffer.concat([tail, chunk]).subarray(-expected.length);
    }
  });
  socket.on('end', common.mustCall(() => {
    assert.strictEqual(received, direct.length + expected.length);
    assert.deepStrictEqual(tail, expected);
    clearTimeout(timer);
    hook.disable();
    server.close(common.mustCall());
  }));
  socket.pause();
  startWrites();
}));

function startWrites() {
  if (started || accepted === undefined || client === undefined ||
      client.connecting) {
    return;
  }
  started = true;

  // Keep a native write queued without marking Writable as busy. The
  // following pure-string vector must then survive solely through the
  // coalesced BackingStore owned by its native WriteWrap.
  const req = new WriteWrap();
  req.handle = client._handle;
  req.oncomplete = common.mustCall(
    (status) => assert.strictEqual(status, 0),
  );
  req.async = false;
  req.bytes = 0;
  req.buffer = direct;
  assert.strictEqual(client._handle.writeBuffer(req, direct), 0);
  req.async = !!streamBaseState[kLastWriteWasAsync];
  assert.strictEqual(req.async, true);

  client[kInternalWritev](vector, common.mustCall(() => client.end()));
  const coalescedWrap = writeWraps.at(-1);
  assert.notStrictEqual(coalescedWrap, req);
  assert.notStrictEqual(coalescedWrap, undefined);
  assert.strictEqual(coalescedWrap.async, true);
  assert.strictEqual(coalescedWrap.bytes, expected.length);
  assert.strictEqual(coalescedWrap.buffer, null);

  vector = null;
  global.gc();
  setImmediate(() => accepted.resume());
}

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  client = net.createConnection({
    host: common.localhostIPv4,
    port: server.address().port,
  }, common.mustCall(startWrites));
  client.on('error', common.mustNotCall());
}));
