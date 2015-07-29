'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.opensslCli) {
  console.log('1..0 # Skipped: node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
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
  cert:loadPEM('agent2-cert')
}, null).listen(common.PORT, function() {
  var args = ['s_client', '-quiet', '-tls1_1',
              '-connect', '127.0.0.1:' + common.PORT];

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
