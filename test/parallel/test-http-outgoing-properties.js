'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;

{
  const msg = new OutgoingMessage();
  assert.strictEqual(msg.writableObjectMode, false);
}

{
  const msg = new OutgoingMessage();
  assert(msg.writableHighWaterMark > 0);
}

{
  const server = http.createServer(common.mustCall(function(req, res) {
    const hwm = req.socket.writableHighWaterMark;
    assert.strictEqual(res.writableHighWaterMark, hwm);

    assert.strictEqual(res.writableLength, 0);
    res.write('');
    const len = res.writableLength;
    res.write('asd');
    assert.strictEqual(res.writableLength, len + 8);
    res.end();
    res.on('finish', common.mustCall(() => {
      assert.strictEqual(res.writableLength, 0);
      server.close();
    }));
  }));

  server.listen(0);

  server.on('listening', common.mustCall(function() {
    const clientRequest = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });
    clientRequest.end();
  }));
}

{
  const msg = new OutgoingMessage();
  msg._implicitHeader = function() {};
  assert.strictEqual(msg.writableLength, 0);
  msg.write('asd');
  assert.strictEqual(msg.writableLength, 3);
}

{
  const server = http.createServer((req, res) => {
    res.end();
    server.close();
  });

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });

    assert.strictEqual(req.path, '/');
    assert.strictEqual(req.method, 'GET');
    assert.strictEqual(req.host, 'localhost');
    assert.strictEqual(req.protocol, 'http:');
    req.end();
  }));
}
