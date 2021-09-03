'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const reqError = new Error('foo');
const socketError = new Error('bar');

new http.Server().listen(0, common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  })
    .on('socket', common.mustCall((socket) => {
      req.onSocket(socket, reqError);
      socket.destroy(socketError);
    }))
    .once('error', common.mustCall((err) => {
      assert.strictEqual(err, socketError);
      req.once('error', common.mustCall((err) => {
        assert.strictEqual(err.message, 'foo');
        assert.deepStrictEqual(err.errors, [reqError, socketError]);
      }));
    }));

  req.end();

  this.unref();
}));
