'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { Writable, pipeline } = require('stream');

for (const method of ['emit', 'emitAndDestroy', 'socketDestroy', 'requestDestroy', 'reset', 'close']) {
  let error = method === 'close' || method === 'reset' ? null : new Error('Socket failure');
  let serverSocket;
  const server = http.createServer(common.mustCall((req, res) => {
    serverSocket = req.socket;
    res.writeHead(200, { 'Content-Length': 100 });
    res.write('partial body');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({
      port: server.address().port,
      agent: false,
    }, common.mustCall((res) => {
      res.on('aborted', common.mustCall());
      res.on('close', common.mustCall());
      pipeline(res, new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      }), common.mustCall((err) => {
        assert.strictEqual(res.complete, false);
        assert.strictEqual(res.aborted, true);
        assert.strictEqual(err.code, 'ECONNRESET');
        assert.strictEqual(err.message, 'aborted');
        assert.strictEqual(res.errored, err);
        if (error) {
          assert.notStrictEqual(err, error);
          assert.strictEqual(err.cause, error);
          assert.deepStrictEqual(Object.getOwnPropertyDescriptor(err, 'cause'), {
            value: error,
            writable: true,
            enumerable: false,
            configurable: true,
          });
        } else {
          assert.strictEqual(Object.hasOwn(err, 'cause'), false);
        }
        server.close(common.mustCall());
      }));

      switch (method) {
        case 'emit':
        case 'emitAndDestroy':
          // TLSSocket can emit an error without setting socket.errored.
          req.socket.emit('error', error);
          break;
        case 'socketDestroy':
          req.socket.destroy(error);
          break;
        case 'requestDestroy':
          req.destroy(error);
          break;
        case 'reset':
          serverSocket.resetAndDestroy();
          break;
        case 'close':
          req.socket.destroy();
          break;
      }
    }));
    req.on('error', method !== 'close' ? common.mustCall((err) => {
      if (method === 'reset') {
        assert.strictEqual(err.code, 'ECONNRESET');
        assert.strictEqual(err.syscall, 'read');
        error = err;
      } else {
        assert.strictEqual(err, error);
      }
      if (method === 'emitAndDestroy')
        req.destroy();
    }) : common.mustNotCall());
    req.on('close', common.mustCall());
  }));
}
