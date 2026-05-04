'use strict';
// Regression test: when a pipelined ServerResponse (whose writes were
// buffered in outputData while the socket belonged to a previous response)
// is finally assigned its socket and flushed, 'drain' must not be emitted
// until the socket's own buffer has actually drained. Previously,
// socketOnDrain was called synchronously from _flushOutput via _onPendingData
// and emitted 'drain' even though the bytes we just wrote were still sitting
// in the socket's writable buffer, so res.writableLength was non-zero.

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

let step = 0;

const server = http.createServer(common.mustCall((req, res) => {
  step++;

  if (step === 1) {
    // Keep the first response open briefly so the second is queued with
    // res.socket === null.
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    setTimeout(() => res.end('ok'), 50);
    return;
  }

  // Second (pipelined) response - queued in state.outgoing, no socket yet.
  assert.strictEqual(res.socket, null);

  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write past the response's highWaterMark so res.write() returns false
  // and kNeedDrain is set. Data is buffered in outputData.
  const chunk = Buffer.alloc(16 * 1024, 'x');
  while (res.write(chunk));
  assert.strictEqual(res.writableNeedDrain, true);

  res.on('drain', common.mustCall(() => {
    assert.strictEqual(
      res.writableLength, 0,
      `'drain' fired with writableLength=${res.writableLength}`,
    );
    res.end();
    server.close();
  }));
}, 2));

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const client = net.connect(port);
  client.write(
    `GET /1 HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n` +
    `GET /2 HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n`,
  );
  client.resume();
  client.on('error', () => {});
}));
