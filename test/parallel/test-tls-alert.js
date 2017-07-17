'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

const assert = require('assert');
const { spawn } = require('child_process');
const tls = require('tls');
const fixtures = require('../common/fixtures');

let success = false;

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
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
