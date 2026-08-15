'use strict';

// Regression test: when _flush() hands buffered data to a socket whose
// writableHighWaterMark is higher than the OutgoingMessage's kHighWaterMark,
// drain must still fire.  Previously, _flush() gated drain emission on
// writableLength === 0, which included socket.writableLength — but the
// socket was never backpressured (data < socket HWM), so drain never fired.
//
// See: https://github.com/nodejs/node/issues/64680

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Server that delays reading to keep socket.writableLength > 0 during flush.
const server = http.createServer(common.mustCall((req, res) => {
  setTimeout(() => {
    req.resume();
    req.on('end', () => res.end('ok'));
  }, 500);
}, 2));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const agent = new http.Agent({ keepAlive: true });

  // Request A: creates socket with HWM=2MB.
  http.request({
    host: 'localhost', port, method: 'POST', agent,
    highWaterMark: 2 * 1024 * 1024,
  }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall(() => {
      // Wait for socket to return to pool.
      setTimeout(common.mustCall(() => {
        // Request B: default HWM (64KB), reuses socket (HWM=2MB).
        // Write 500KB: above OM HWM (64KB), below socket HWM (2MB).
        const reqB = http.request({
          host: 'localhost', port, method: 'POST', agent,
        }, common.mustCall((res2) => {
          res2.resume();
          res2.on('end', common.mustCall(() => {
            server.close();
          }));
        }));

        const result = reqB.write(Buffer.alloc(500 * 1024));
        assert.strictEqual(result, false);

        // Drain must fire — no deadlock.
        reqB.on('drain', common.mustCall(() => {
          reqB.end();
        }));
      }), 100);
    }));
  })).end('x');
}));
