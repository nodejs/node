var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var Buffer = require('buffer').Buffer;
var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var buf = new Buffer(1024 * 1024);
var sent = 0;
var received = 0;

var server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  for (var i = 0; i < 50; i++) {
    res.write(buf);
  }
  res.end();
});

server.listen(common.PORT, function() {
  var resumed = false;
  var req = https.request({
    method: 'POST',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.read(0);

    setTimeout(function() {
      // Read buffer should be somewhere near high watermark
      // (i.e. should not leak)
      assert(res._readableState.length < 100 * 1024);
      process.exit(0);
    }, 2000);
  });
  req.end();
});
