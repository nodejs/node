'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const tcp = net.Server(common.mustCall((s) => {
  tcp.close();

  let buf = '';
  s.setEncoding('utf8');
  s.on('data', function(d) {
    buf += d;
  });

  s.on('end', common.mustCall(function() {
    console.error('SERVER: end', buf);
    assert.strictEqual(buf, "L'État, c'est moi");
    s.end();
  }));
}));

tcp.listen(0, common.mustCall(function() {
  const socket = net.Stream({ highWaterMark: 0 });

  let connected = false;
  assert.strictEqual(socket.pending, true);
  socket.connect(this.address().port, common.mustCall(() => connected = true));

  assert.strictEqual(socket.pending, true);
  assert.strictEqual(socket.connecting, true);
  assert.strictEqual(socket.readyState, 'opening');

  // Write a string that contains a multi-byte character sequence to test that
  // `bytesWritten` is incremented with the # of bytes, not # of characters.
  const a = "L'État, c'est ";
  const b = 'moi';

  // We're still connecting at this point so the datagram is first pushed onto
  // the connect queue. Make sure that it's not added to `bytesWritten` again
  // when the actual write happens.
  const r = socket.write(a, common.mustCall((er) => {
    console.error('write cb');
    assert.ok(connected);
    assert.strictEqual(socket.bytesWritten, Buffer.from(a + b).length);
    assert.strictEqual(socket.pending, false);
  }));
  socket.on('close', common.mustCall(() => {
    assert.strictEqual(socket.pending, true);
  }));

  assert.strictEqual(socket.bytesWritten, Buffer.from(a).length);
  assert.strictEqual(r, false);
  socket.end(b);

  assert.strictEqual(socket.readyState, 'opening');
}));
