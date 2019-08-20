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
  assert.strictEqual(msg.writableLength, 7);
}
