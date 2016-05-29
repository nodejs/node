'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var bufSize = 1024 * 1024;
var sent = 0;
var received = 0;

var server = tls.Server(options, function(socket) {
  socket.pipe(socket);
  socket.on('data', function(c) {
    console.error('data', c.length);
  });
});

server.listen(0, function() {
  var resumed = false;
  var client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    console.error('connected');
    client.pause();
    console.error('paused');
    send();
    function send() {
      console.error('sending');
      var ret = client.write(Buffer.allocUnsafe(bufSize));
      console.error('write => %j', ret);
      if (false !== ret) {
        console.error('write again');
        sent += bufSize;
        assert.ok(sent < 100 * 1024 * 1024); // max 100MB
        return process.nextTick(send);
      }
      sent += bufSize;
      console.error('sent: ' + sent);
      resumed = true;
      client.resume();
      console.error('resumed', client);
    }
  });
  client.on('data', function(data) {
    console.error('data');
    assert.ok(resumed);
    received += data.length;
    console.error('received', received);
    console.error('sent', sent);
    if (received >= sent) {
      console.error('received: ' + received);
      client.end();
      server.close();
    }
  });
});

process.on('exit', function() {
  assert.equal(sent, received);
});
