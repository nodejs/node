'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var stream = require('stream');
var fs = require('fs');
var net = require('net');

var connected = {
  client: 0,
  server: 0
};

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  console.log('new client');
  connected.server++;
  c.end('ohai');
}).listen(0, function() {
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

  var socket = tls.connect({
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
  assert.equal(connected.client, 1);
  assert.equal(connected.server, 1);
});
