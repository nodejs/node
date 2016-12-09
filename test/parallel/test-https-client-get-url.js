'use strict';
const common = require('../common');
// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = https.createServer(options, common.mustCall(function(req, res) {
  assert.equal('GET', req.method);
  assert.equal('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
}));

server.listen(0, function() {
  https.get(`https://127.0.0.1:${this.address().port}/foo?bar`);
});
