'use strict';
// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var seen_req = false;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = https.createServer(options, function(req, res) {
  assert.equal('GET', req.method);
  assert.equal('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
  seen_req = true;
});

server.listen(common.PORT, function() {
  https.get('https://127.0.0.1:' + common.PORT + '/foo?bar');
});

process.on('exit', function() {
  assert(seen_req);
});
