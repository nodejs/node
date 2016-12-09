'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');
const stream = require('stream');
const net = require('net');

var server;
var cert_dir = common.fixturesDir;
var options = { key: fs.readFileSync(cert_dir + '/test_key.pem'),
                cert: fs.readFileSync(cert_dir + '/test_cert.pem'),
                ca: [ fs.readFileSync(cert_dir + '/test_ca.pem') ],
                ciphers: 'AES256-GCM-SHA384' };
var content = 'hello world';
var recv_bufs = [];
var send_data = '';
server = tls.createServer(options, function(s) {
  s.on('data', function(c) {
    recv_bufs.push(c);
  });
});
server.listen(0, function() {
  var raw = net.connect(this.address().port);

  var pending = false;
  raw.on('readable', function() {
    if (pending)
      p._read();
  });

  var p = new stream.Duplex({
    read: function read() {
      pending = false;

      var chunk = raw.read();
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

  var socket = tls.connect({
    socket: p,
    rejectUnauthorized: false
  }, function() {
    for (var i = 0; i < 50; ++i) {
      socket.write(content);
      send_data += content;
    }
    socket.end();
    server.close();
  });
});

process.on('exit', function() {
  var recv_data = (Buffer.concat(recv_bufs)).toString();
  assert.strictEqual(send_data, recv_data);
});
