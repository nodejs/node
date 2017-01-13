'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');

const fs = require('fs');

const key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
const cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

// number of bytes discovered empirically to trigger the bug
const data = Buffer.alloc(1024 * 32 + 1);

httpsTest();

function httpsTest() {
  const sopt = { key: key, cert: cert };

  const server = https.createServer(sopt, function(req, res) {
    res.setHeader('content-length', data.length);
    res.end(data);
    server.close();
  });

  server.listen(0, function() {
    const opts = { port: this.address().port, rejectUnauthorized: false };
    https.get(opts).on('response', function(res) {
      test(res);
    });
  });
}


const test = common.mustCall(function(res) {
  res.on('end', common.mustCall(function() {
    assert.strictEqual(res._readableState.length, 0);
    assert.strictEqual(bytes, data.length);
  }));

  // Pause and then resume on each chunk, to ensure that there will be
  // a lone byte hanging out at the very end.
  let bytes = 0;
  res.on('data', function(chunk) {
    bytes += chunk.length;
    this.pause();
    setTimeout(this.resume.bind(this), 1);
  });
});
