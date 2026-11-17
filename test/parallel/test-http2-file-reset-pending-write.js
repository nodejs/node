// Flags: --expose-gc
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const http2 = require('http2');
const { duplexPair } = require('stream');
const { setImmediate } = require('timers/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const filename = tmpdir.resolve('http2-file');
fs.writeFileSync(filename, Buffer.alloc(1024 * 1024));

async function test(useFD) {
  const [clientSide, serverSide] = duplexPair();
  let reset = false;
  let pendingWrite;
  const originalWrite = serverSide._write;
  serverSide._write = common.mustCallAtLeast(function(buf, enc, cb) {
    originalWrite.call(this, buf, enc, common.mustCall(async () => {
      // Deliver the data, but allow the client to reset the stream before
      // completing the corresponding native write.
      await setImmediate();
      if (reset) {
        assert.strictEqual(pendingWrite, undefined);
        pendingWrite = cb;
      } else {
        cb();
      }
    }));
  });

  const server = http2.createServer();
  const fd = useFD ? fs.openSync(filename, 'r') : undefined;
  let streamClosed;
  const closed = new Promise((resolve) => { streamClosed = resolve; });
  server.on('stream', common.mustCall((stream) => {
    stream.on('close', common.mustCall(streamClosed));
    if (useFD)
      stream.respondWithFD(fd);
    else
      stream.respondWithFile(filename);
  }));
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost', {
    createConnection: () => clientSide,
    settings: { initialWindowSize: 1024 * 1024 },
  });
  client.on('connect', common.mustCall(() => {
    client.setLocalWindowSize(1024 * 1024);
  }));
  const req = client.request();
  req.once('data', common.mustCall(() => {
    reset = true;
    req.close(http2.constants.NGHTTP2_CANCEL);
  }));
  req.resume();

  await closed;
  // Let deferred native stream cleanup and unpipe callbacks run, then collect
  // the wrapper while its file write is still awaiting completion.
  for (let i = 0; i < 10; i++) {
    await setImmediate();
    global.gc();
  }
  assert.strictEqual(typeof pendingWrite, 'function');
  reset = false;
  pendingWrite();
  pendingWrite = undefined;
  await setImmediate();

  client.destroy();
  server.close();
  if (useFD)
    fs.closeSync(fd);
}

(async () => {
  await test(false);
  await test(true);
})().then(common.mustCall());
