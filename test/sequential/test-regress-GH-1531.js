var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var gotCallback = false;

var server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

server.listen(common.PORT, function() {
  console.error('listening');
  https.get({
    agent: false,
    path: '/',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    console.error(res.statusCode, res.headers);
    gotCallback = true;
    res.resume();
    server.close();
  }).on('error', function(e) {
    console.error(e.stack);
    process.exit(1);
  });
});

process.on('exit', function() {
  assert.ok(gotCallback);
  console.log('ok');
});
