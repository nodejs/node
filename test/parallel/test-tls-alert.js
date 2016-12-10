'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var spawn = require('child_process').spawn;

var success = false;

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var server = tls.Server({
  secureProtocol: 'TLSv1_2_server_method',
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
}, null).listen(0, function() {
  var args = ['s_client', '-quiet', '-tls1_1',
              '-connect', `127.0.0.1:${this.address().port}`];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  var client = spawn(common.opensslCli, args);
  var out = '';
  client.stderr.setEncoding('utf8');
  client.stderr.on('data', function(d) {
    out += d;
    if (/SSL alert number 70/.test(out)) {
      success = true;
      server.close();
    }
  });
});
process.on('exit', function() {
  assert(success);
});
