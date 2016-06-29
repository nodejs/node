'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

// number of bytes discovered empirically to trigger the bug
var data = Buffer.allocUnsafe(1024 * 32 + 1);

httpsTest();

function httpsTest() {
  var sopt = { key: key, cert: cert };

  var server = https.createServer(sopt, function(req, res) {
    res.setHeader('content-length', data.length);
    res.end(data);
    server.close();
  });

  server.listen(0, function() {
    var opts = { port: this.address().port, rejectUnauthorized: false };
    https.get(opts).on('response', function(res) {
      test(res);
    });
  });
}


function test(res) {
  res.on('end', function() {
    assert.equal(res._readableState.length, 0);
    assert.equal(bytes, data.length);
    console.log('ok');
  });

  // Pause and then resume on each chunk, to ensure that there will be
  // a lone byte hanging out at the very end.
  var bytes = 0;
  res.on('data', function(chunk) {
    bytes += chunk.length;
    this.pause();
    setTimeout(this.resume.bind(this));
  });
}
