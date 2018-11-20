'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const net = require('net');
const stream = require('stream');
const tls = require('tls');

const connected = {
  client: 0,
  server: 0
};

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, function(c) {
  console.log('new client');
  connected.server++;
  c.end('ohai');
}).listen(0, function() {
  const raw = net.connect(this.address().port);

  let pending = false;
  raw.on('readable', function() {
    if (pending)
      p._read();
  });

  const p = new stream.Duplex({
    read: function read() {
      pending = false;

      const chunk = raw.read();
      if (chunk) {
        console.log('read', chunk);
        this.push(chunk);
      } else {
        pending = true;
      }
    },
    write: function write(data, enc, cb) {
      console.log('write', data, enc);
      raw.write(data, enc, cb);
    }
  });

  const socket = tls.connect({
    socket: p,
    rejectUnauthorized: false
  }, function() {
    console.log('client secure');

    connected.client++;

    socket.end('hello');
    socket.resume();
    socket.destroy();
  });

  socket.once('close', function() {
    console.log('client close');
    server.close();
  });
});

process.once('exit', function() {
  assert.strictEqual(connected.client, 1);
  assert.strictEqual(connected.server, 1);
});
