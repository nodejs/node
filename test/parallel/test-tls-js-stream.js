'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const net = require('net');
const stream = require('stream');
const tls = require('tls');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, common.mustCall(function(c) {
  console.log('new client');

  c.resume();
  c.end('ohai');
})).listen(0, common.mustCall(function() {
  const raw = net.connect(this.address().port);

  let pending = false;
  raw.on('readable', function() {
    if (pending)
      p._read();
  });

  raw.on('end', function() {
    p.push(null);
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
  }, common.mustCall(function() {
    console.log('client secure');

    socket.resume();
    socket.end('hello');
  }));

  socket.once('close', function() {
    console.log('client close');
    server.close();
  });
}));
