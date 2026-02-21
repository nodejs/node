'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(handle);

const N = 100;
const buf = Buffer.alloc(2, 'a');

server.listen(0, function() {
  const conn = net.connect(this.address().port);

  conn.on('connect', () => {
    let res = true;
    let i = 0;
    for (; i < N && res; i++) {
      conn.cork();
      conn.write(buf);
      res = conn.write(buf);
      conn.uncork();
    }
    assert.strictEqual(i, N);
    conn.end();
  });
});

function handle(socket) {
  socket.resume();
  socket.on('error', common.mustNotCall())
        .on('close', common.mustCall(() => server.close()));
}
