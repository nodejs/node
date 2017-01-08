'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const body = 'hello world\n';

const httpsServer = https.createServer(options, function(req, res) {
  res.on('finish', function() {
    assert.strictEqual(typeof req.connection.bytesWritten, 'number');
    assert(req.connection.bytesWritten > 0);
    httpsServer.close();
    console.log('ok');
  });
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(body);
});

httpsServer.listen(0, function() {
  https.get({
    port: this.address().port,
    rejectUnauthorized: false
  });
});
