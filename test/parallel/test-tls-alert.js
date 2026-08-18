// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const {
  hasOpenSSL,
  hasFIPS,
  opensslCli,
} = require('../common/crypto');

if (!opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
}

const assert = require('assert');
const { execFile } = require('child_process');
const tls = require('tls');
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
};
if (hasFIPS(3)) {
  serverOptions.minVersion = 'TLSv1.3';
  serverOptions.maxVersion = 'TLSv1.3';
} else {
  serverOptions.secureProtocol = 'TLSv1_2_server_method';
}

const server = tls.Server(serverOptions, null).listen(0, common.mustCall(() => {
  if (process.features.openssl_is_boringssl) {
    let gotClientError = false;
    let gotServerError = false;
    function maybeClose() {
      if (gotClientError && gotServerError)
        server.close();
    }

    server.once('tlsClientError', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_UNSUPPORTED_PROTOCOL');
      gotServerError = true;
      maybeClose();
    }));

    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      secureProtocol: 'TLSv1_1_method',
    }, common.mustNotCall());
    client.once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_TLSV1_ALERT_PROTOCOL_VERSION');
      gotClientError = true;
      maybeClose();
    }));
    return;
  }

  const args = ['s_client', '-quiet', hasFIPS(3) ? '-tls1_2' : '-tls1_1',
                '-cipher', hasFIPS(3) ? 'DEFAULT' :
                  (hasOpenSSL(3, 1) ? 'DEFAULT:@SECLEVEL=0' : 'DEFAULT'),
                '-connect', `127.0.0.1:${server.address().port}`];

  execFile(opensslCli, args, common.mustCall((err, _, stderr) => {
    assert.strictEqual(err.code, 1);
    assert.match(stderr, /SSL alert number 70/);
    server.close();
  }));
}));
