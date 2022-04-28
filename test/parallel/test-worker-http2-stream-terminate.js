'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const makeDuplexPair = require('../common/duplexpair');
const { Worker, isMainThread, parentPort } = require('worker_threads');

// This test ensures that workers can be terminated without error while
// stream activity is ongoing, in particular the C++ function
// ReportWritesToJSStreamListener::OnStreamAfterReqFinished.

if (isMainThread) {
  const sab = new SharedArrayBuffer(4);
  const terminate = new Int32Array(sab);

  const w = new Worker(__filename);
  w.postMessage(sab);
  process.nextTick(() => {
    Atomics.wait(terminate, 0, 0);
    setImmediate(() => w.terminate());
  });
  return;
}

parentPort.on('message', (sab) => {
  const terminate = new Int32Array(sab);
  const server = http2.createServer();
  let i = 0;
  server.on('stream', (stream, headers) => {
    if (i === 1) {
      Atomics.store(terminate, 0, 1);
      Atomics.notify(terminate, 0, 1);
    }
    i++;

    stream.end('');
  });

  const { clientSide, serverSide } = makeDuplexPair();
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost:80', {
    createConnection: () => clientSide,
  });

  function makeReq() {
    for (let i = 0; i < 3; i++) {
      client.request().end();
    }
    setImmediate(makeReq);
  }
  makeReq();

});
