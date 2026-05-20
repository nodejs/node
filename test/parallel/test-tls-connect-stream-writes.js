'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const stream = require('stream');
const net = require('net');
const fixtures = require('../common/fixtures');

const options = { key: fixtures.readKey('rsa_private.pem'),
                  cert: fixtures.readKey('rsa_cert.crt'),
                  ca: [ fixtures.readKey('rsa_ca.crt') ],
                  ciphers: 'AES256-GCM-SHA384' };
const content = 'hello world';
const recv_bufs = [];
let send_data = '';
const server = tls.createServer(options, function(s) {
  s.on('data', function(c) {
    recv_bufs.push(c);
  });
});
server.listen(0, function() {
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
        this.push(chunk);
      } else {
        pending = true;
      }
    },
    write: function write(data, enc, cb) {
      raw.write(data, enc, cb);
    }
  });

  const socket = tls.connect({
    socket: p,
    rejectUnauthorized: false
  }, function() {
    for (let i = 0; i < 50; ++i) {
      socket.write(content);
      send_data += content;
    }
    socket.end();
    server.close();
  });
});

process.on('exit', function() {
  const recv_data = (Buffer.concat(recv_bufs)).toString();
  assert.strictEqual(send_data, recv_data);
});
