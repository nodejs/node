'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const spawn = require('child_process').spawn;

let success = false;

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

const server = tls.Server({
  secureProtocol: 'TLSv1_2_server_method',
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
}, null).listen(0, function() {
  const args = ['s_client', '-quiet', '-tls1_1',
                '-connect', `127.0.0.1:${this.address().port}`];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  const client = spawn(common.opensslCli, args);
  let out = '';
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
