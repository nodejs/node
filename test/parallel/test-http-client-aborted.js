'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const EventEmitter = require('events')

{
  const server = http.createServer(common.mustCall(function(req, res) {
    req.on('aborted', common.mustCall(function() {
      assert.strictEqual(this.aborted, true);
      server.close();
    }));
    assert.strictEqual(req.aborted, false);
    res.write('hello');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({
      port: server.address().port,
      headers: { connection: 'keep-alive' }
    }, common.mustCall((res) => {
      req.destroy(new Error('kaboom'));
      req.on('error', common.mustCall());
      assert.strictEqual(req.destroyed, true);
      assert.strictEqual(req.aborted, true);
    }));
  }));
}

{
  const req = http.request({
    createConnection: (_, callback) => {
      process.nextTick(callback, new Error('kaboom'));
    }
  });
  req.on('socket', common.mustNotCall());
  req.destroy();
  req.on('error', common.mustCall(common.expectsError({
    message: 'kaboom'
  })));
}

{
  // Make sure socket is destroyed if req.destroy
  // is called before socket is ready.
  const dummySocket = new EventEmitter();
  dummySocket.destroy = common.mustCall();
  dummySocket.on('free', common.mustNotCall());
  const req = http.request({
    createConnection: common.mustCall((_, callback) => {
      process.nextTick(callback, null, dummySocket);
    })
  });
  req.on('socket', common.mustNotCall());
  req.destroy();
}
