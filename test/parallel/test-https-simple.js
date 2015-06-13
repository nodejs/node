'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var fs = require('fs');
var exec = require('child_process').exec;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqCount = 0;
var body = 'hello world\n';

var server = https.createServer(options, function(req, res) {
  reqCount++;
  console.log('got request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});


server.listen(common.PORT, function() {
  var cmd = 'curl --insecure https://127.0.0.1:' + common.PORT + '/';
  console.error('executing %j', cmd);
  exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    common.error(common.inspect(stdout));
    assert.equal(body, stdout);

    // Do the same thing now without --insecure
    // The connection should not be accepted.
    var cmd = 'curl https://127.0.0.1:' + common.PORT + '/';
    console.error('executing %j', cmd);
    exec(cmd, function(err, stdout, stderr) {
      assert.ok(err);
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(1, reqCount);
});
