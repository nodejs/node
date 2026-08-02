// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { kRawWritev } = require('internal/streams/utils');

function runBackpressure(contentLength = false) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      if (contentLength) {
        res.setHeader('Content-Length', 1);
      }
      assert.strictEqual(res.write('A'), false);
      assert.strictEqual(res.writableNeedDrain, true);
      res.once('drain', common.mustCall(() => {
        assert.strictEqual(res.writableNeedDrain, false);
        res.end();
      }));
    }));

    server.on('connection', common.mustCall((socket) => {
      socket._writableState.highWaterMark = 100;
    }));
    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      }, common.mustCall((response) => {
        let body = '';
        response.setEncoding('utf8');
        response.on('data', (chunk) => body += chunk);
        response.on('end', common.mustCall(() => {
          assert.strictEqual(body, 'A');
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
    }));
  });
}

function runClientBufferedDrain() {
  return new Promise((resolve, reject) => {
    let accepted = false;
    const server = net.createServer(common.mustCall((socket) => {
      accepted = true;
      socket.on('error', reject);
      socket.resume();
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        headers: { 'Content-Length': 200 },
      });
      request.on('error', () => {});
      request.on('socket', common.mustCall((socket) => {
        socket._writableState.highWaterMark = 64;
        const originalWritev = socket[kRawWritev];
        let completeFirstWrite;

        socket[kRawWritev] = common.mustCall((chunks, callback) => {
          completeFirstWrite = callback;
        });
        socket.once('connect', common.mustCall(() => {
          const start = common.mustCall(() => {
            assert.strictEqual(request.write('A'.repeat(100)), false);
            setImmediate(common.mustCall(() => {
              assert.strictEqual(typeof completeFirstWrite, 'function');
              request.cork();
              assert.strictEqual(request.write('B'.repeat(100)), false);

              let drained = false;
              request.once('drain', common.mustCall(() => {
                drained = true;
                assert.strictEqual(request.writableLength, 0);
                socket.destroy();
                server.close(common.mustCall(resolve));
              }));

              completeFirstWrite();
              assert.strictEqual(drained, false);
              assert.strictEqual(request.writableLength, 100);

              socket[kRawWritev] = originalWritev;
              request.uncork();
            }));
          });
          if (accepted) start();
          else server.once('connection', start);
        }));
      }));
    }));
  });
}

function runAsyncEncodedDrain() {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      const body = '\ud83d\ude00'.repeat(32);
      res._implicitHeader();
      const length = Buffer.byteLength(body, 'utf8');
      const pending = res._header.length + length +
        length.toString(16).length + 4;
      res.socket._writableState.highWaterMark = pending;

      const originalWritev = res.socket[kRawWritev];
      let completeWrite;
      res.socket[kRawWritev] = common.mustCall((chunks, callback) => {
        completeWrite = callback;
      });

      assert.strictEqual(res.write(body, 'utf8'), false);
      assert.strictEqual(res.writableLength, pending);
      let drained = false;
      res.once('drain', common.mustCall(() => {
        drained = true;
        assert.strictEqual(res.writableLength, 0);
      }));

      setImmediate(common.mustCall(() => {
        assert.strictEqual(typeof completeWrite, 'function');
        completeWrite();
        setImmediate(common.mustCall(() => {
          assert.strictEqual(drained, true);
          res.socket[kRawWritev] = originalWritev;
          res.socket.destroy();
          server.close(common.mustCall(resolve));
        }));
      }));
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      socket.on('error', () => {});
      socket.on('connect', common.mustCall(() => {
        socket.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
      }));
      socket.resume();
    }));
  });
}

function runPipelinedBackpressure() {
  return new Promise((resolve, reject) => {
    let requests = 0;
    const server = http.createServer(common.mustCall((req, res) => {
      requests++;
      if (requests === 1) {
        assert.strictEqual(res.write('A'.repeat(1000)), false);
        return;
      }

      // The active response has exceeded its high-water mark in the
      // message-level auto-cork buffer. Parsing must pause before the next
      // network read, and queuing an inactive response must not resume it.
      assert.strictEqual(req.socket._paused, true);
      res.end('B');
      assert.strictEqual(req.socket._paused, true);
      req.socket.destroy();
    }, 2));

    server.on('connection', common.mustCall((socket) => {
      socket._writableState.highWaterMark = 100;
    }));
    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      socket.on('error', reject);
      socket.on('close', common.mustCall(() => {
        server.close(common.mustCall(resolve));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.write(
          'GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n' +
          'GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n',
        );
      }));
    }));
  });
}

function runQueuedBackpressure(flushHeaders = false) {
  return new Promise((resolve, reject) => {
    let requests = 0;
    let socket;
    const server = http.createServer(common.mustCall((req, res) => {
      requests++;
      if (requests === 1) {
        return;
      }
      if (requests === 2) {
        res.cork();
        if (flushHeaders) {
          let trackedPendingData = 0;
          const updatePendingData = res._onPendingData;
          res._onPendingData = (delta) => {
            trackedPendingData += delta;
            updatePendingData(delta);
          };
          res.write('A');
          const pending = res.writableLength;
          req.socket._writableState.highWaterMark = pending + 1;
          res.flushHeaders();
          assert.strictEqual(res.writableLength, pending);
          assert.strictEqual(trackedPendingData, pending);
        } else {
          assert.strictEqual(res.write('A'.repeat(1000)), false);
        }
        setImmediate(() => {
          socket.write('GET /third HTTP/1.1\r\nHost: localhost\r\n\r\n');
        });
        return;
      }

      if (flushHeaders) {
        // flushHeaders() transfers the header from the message-level buffer
        // to outputData. It must not count the same bytes in both owners.
        assert.strictEqual(req.socket._paused, false);
      } else {
        // The explicitly corked second response is not assigned to the socket,
        // but its message-level buffer still belongs to this connection.
        assert.strictEqual(req.socket._paused, true);
      }
      req.socket.destroy();
    }, 3));

    server.on('connection', common.mustCall((connection) => {
      if (!flushHeaders) {
        connection._writableState.highWaterMark = 100;
      }
    }));
    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      socket.on('error', reject);
      socket.on('close', common.mustCall(() => {
        server.close(common.mustCall(resolve));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.write(
          'GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n' +
          'GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n',
        );
      }));
    }));
  });
}

async function main() {
  await runBackpressure();
  await runBackpressure(true);
  await runClientBufferedDrain();
  await runAsyncEncodedDrain();
  await runPipelinedBackpressure();
  await runQueuedBackpressure();
  await runQueuedBackpressure(true);
}

main().then(common.mustCall());
