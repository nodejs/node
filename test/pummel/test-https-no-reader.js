'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const Buffer = require('buffer').Buffer;
const fs = require('fs');
const path = require('path');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const buf = Buffer.allocUnsafe(1024 * 1024);

const server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  for (let i = 0; i < 50; i++) {
    res.write(buf);
  }
  res.end();
});

server.listen(common.PORT, function() {
  const req = https.request({
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
