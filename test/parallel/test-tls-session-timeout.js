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
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const { spawn } = require('child_process');

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');


// WARNING!
// This is only a POC test.
// Unfortunately, it is useless because with '-no_tickets' option not enabled,
// 'newSession' and 'resumeSession' event callbacks are not called.
doTest();

function doTest() {
  const key = fixtures.readKey('rsa_private.pem');
  const cert = fixtures.readKey('rsa_cert.crt');
  const options = {
    key,
    cert,
    ca: [cert],
    requestCert: true,
    rejectUnauthorized: false,
    secureProtocol: 'TLS_method',
  };
  let session;

  let newSessions = 0;
  let resumedSessions = 0;

  const server = tls.createServer(options, function(cleartext) {
    cleartext.on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    cleartext.end('');
  });

  server.on('newSession', function(id, data, cb) {
    ++newSessions;

    setImmediate(() => {
      session = { id, data };
      cb();
    });
  });

  server.on('resumeSession', function(id, callback) {
    ++resumedSessions;
    assert.ok(session);
    assert.strictEqual(session.id.toString('hex'), id.toString('hex'));

    setImmediate(() => {
      callback(null, session.data);
    });
  });

  server.listen(0, function() {
    const args = [
      's_client',
      '-tls1',
      '-connect', `localhost:${this.address().port}`,
      '-servername', 'ohgod',
      '-key', fixtures.path('keys/rsa_private.pem'),
      '-cert', fixtures.path('keys/rsa_cert.crt'),
      '-no_ticket'
    ];

    function spawnClient(cb) {
      const client = spawn(common.opensslCli, args, {
        stdio: [ 0, 1, 'pipe' ]
      });
      let err = '';
      client.stderr.setEncoding('utf8');
      client.stderr.on('data', function(chunk) {
        err += chunk;
      });

      client.on('exit', common.mustCall(function(code, signal) {
        if (code !== 0) {
          // If SmartOS and connection refused, then retry. See
          // https://github.com/nodejs/node/issues/2663.
          if (common.isSunOS && err.includes('Connection refused')) {
            requestCount = 0;
            spawnClient();
            return;
          }
          assert.fail(`code: ${code}, signal: ${signal}, output: ${err}`);
        }
        assert.strictEqual(code, 0);

        setImmediate(cb);
      }));
    }

    spawnClient(() => {
      // Make the first connection. A new session should be estabilished.
      // newSessions: 0 -> 1
      // resumedSessions: 0
      spawnClient(() => {
        // Make the second connection. A session should be resumed.
        // newSessions: 1
        // resumedSessions: 0 -> 1
        // Set session timeout to 1 second,
        server.setSessionTimeout(1);
        assert.strictEqual(server.getSessionTimeout(), 1);
        // After 2 seconds, session should time out. 
        // newSessions: 1 -> 2
        // resumedSessions: 1
        setTimeout(function () {
          spawnClient(() => {
           server.close();
          });
        }, 2);
      });
    });
  });

  process.on('exit', function() {
  // assert.strictEqual(resumedSessions, 1);
  // assert.strictEqual(newSessions, 2);
  });
};
