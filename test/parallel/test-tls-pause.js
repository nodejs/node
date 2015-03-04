var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
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

server.listen(common.PORT, function() {
  var resumed = false;
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    console.error('connected');
    client.pause();
    common.debug('paused');
    send();
    function send() {
      console.error('sending');
      var ret = client.write(new Buffer(bufSize));
      console.error('write => %j', ret);
      if (false !== ret) {
        console.error('write again');
        sent += bufSize;
        assert.ok(sent < 100 * 1024 * 1024); // max 100MB
        return process.nextTick(send);
      }
      sent += bufSize;
      common.debug('sent: ' + sent);
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
      common.debug('received: ' + received);
      client.end();
      server.close();
    }
  });
});

process.on('exit', function() {
  assert.equal(sent, received);
});
