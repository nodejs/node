'use strict';
var assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    tls = require('tls'),
    stream = require('stream'),
    net = require('net');

var common = require('../common');

var server;
var cert_dir = path.resolve(__dirname, '../fixtures'),
    options = { key: fs.readFileSync(cert_dir + '/test_key.pem'),
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
server.listen(common.PORT, function() {
  var raw = net.connect(common.PORT);

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
