var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var http = require('http');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}

var https = require('https');

var options = {
  tls: true,
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqCount = 0;
var body = 'test';

var server = http.createServer(options, function(req, res) {
  reqCount++;
  res.writeHead(200, {'content-type': 'text/plain'});
  res.end(body);
});

assert(server instanceof https.Server);

server.listen(common.PORT, function() {
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    var data = '';

    res.on('data', function(chunk) {
      data += chunk.toString();
    });

    res.on('end', function() {
      assert.equal(data, body);
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(1, reqCount);
});
