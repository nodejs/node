'use strict';
var common = require('../common');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var assert = require('assert');
var exec = require('child_process').exec;
var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
  ciphers: 'DES-CBC3-SHA'
};

var reply = 'I AM THE WALRUS'; // something recognizable
var response = '';

process.on('exit', function() {
  assert.notEqual(response.indexOf(reply), -1);
});

var server = tls.createServer(options, common.mustCall(function(conn) {
  conn.end(reply);
}));

server.listen(0, '127.0.0.1', function() {
  var cmd = '"' + common.opensslCli + '" s_client -cipher ' + options.ciphers +
            ` -connect 127.0.0.1:${this.address().port}`;

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    cmd += ' -no_rand_screen';

  exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    response = stdout;
    server.close();
  });
});
