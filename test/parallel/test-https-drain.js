'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var bufSize = 1024 * 1024;
var sent = 0;
var received = 0;

var server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  req.pipe(res);
});

server.listen(common.PORT, function() {
  var resumed = false;
  var req = https.request({
    method: 'POST',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    var timer;
    res.pause();
    common.debug('paused');
    send();
    function send() {
      if (req.write(new Buffer(bufSize))) {
        sent += bufSize;
        assert.ok(sent < 100 * 1024 * 1024); // max 100MB
        return process.nextTick(send);
      }
      sent += bufSize;
      common.debug('sent: ' + sent);
      resumed = true;
      res.resume();
      common.debug('resumed');
      timer = setTimeout(function() {
        process.exit(1);
      }, 1000);
    }

    res.on('data', function(data) {
      assert.ok(resumed);
      if (timer) {
        clearTimeout(timer);
        timer = null;
      }
      received += data.length;
      if (received >= sent) {
        common.debug('received: ' + received);
        req.end();
        server.close();
      }
    });
  });
  req.write('a');
  ++sent;
});

process.on('exit', function() {
  assert.equal(sent, received);
});
