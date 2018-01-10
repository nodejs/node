'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
};

const bufSize = 1024 * 1024;
let sent = 0;
let received = 0;

const server = tls.Server(options, function(socket) {
  socket.pipe(socket);
  socket.on('data', function(c) {
    console.error('data', c.length);
  });
});

server.listen(0, function() {
  let resumed = false;
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    console.error('connected');
    client.pause();
    console.error('paused');
    send();
    function send() {
      console.error('sending');
      const ret = client.write(Buffer.allocUnsafe(bufSize));
      console.error(`write => ${ret}`);
      if (false !== ret) {
        console.error('write again');
        sent += bufSize;
        assert.ok(sent < 100 * 1024 * 1024); // max 100MB
        return process.nextTick(send);
      }
      sent += bufSize;
      console.error(`sent: ${sent}`);
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
      console.error(`received: ${received}`);
      client.end();
      server.close();
    }
  });
});

process.on('exit', function() {
  assert.strictEqual(sent, received);
});
