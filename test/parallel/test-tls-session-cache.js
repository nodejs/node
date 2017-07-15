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

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');
const { join } = require('path');
const { spawn } = require('child_process');

const keyFile = join(common.fixturesDir, 'agent.key');
const certFile = join(common.fixturesDir, 'agent.crt');

doTest({ tickets: false }, function() {
  doTest({ tickets: true }, function() {
    doTest({ tickets: false, invalidSession: true }, function() {
      console.error('all done');
    });
  });
});

function doTest(testOptions, callback) {
  const key = fs.readFileSync(keyFile);
  const cert = fs.readFileSync(certFile);
  const options = {
    key: key,
    cert: cert,
    ca: [cert],
    requestCert: true,
    rejectUnauthorized: false
  };
  let requestCount = 0;
  let resumeCount = 0;
  let newSessionCount = 0;
  let session;

  const server = tls.createServer(options, function(cleartext) {
    cleartext.on('error', function(er) {
      // We're ok with getting ECONNRESET in this test, but it's
      // timing-dependent, and thus unreliable. Any other errors
      // are just failures, though.
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    ++requestCount;
    cleartext.end();
  });
  server.on('newSession', function(id, data, cb) {
    ++newSessionCount;
    // Emulate asynchronous store
    setTimeout(function() {
      assert.ok(!session);
      session = {
        id: id,
        data: data
      };
      cb();
    }, 1000);
  });
  server.on('resumeSession', function(id, callback) {
    ++resumeCount;
    assert.ok(session);
    assert.strictEqual(session.id.toString('hex'), id.toString('hex'));

    let data = session.data;

    // Return an invalid session to test Node does not crash.
    if (testOptions.invalidSession) {
      data = Buffer.from('INVALID SESSION');
      session = null;
    }

    // Just to check that async really works there
    setTimeout(function() {
      callback(null, data);
    }, 100);
  });

  server.listen(0, function() {
    const args = [
      's_client',
      '-tls1',
      '-connect', `localhost:${this.address().port}`,
      '-servername', 'ohgod',
      '-key', join(common.fixturesDir, 'agent.key'),
      '-cert', join(common.fixturesDir, 'agent.crt'),
      '-reconnect'
    ].concat(testOptions.tickets ? [] : '-no_ticket');

    // for the performance and stability issue in s_client on Windows
    if (common.isWindows)
      args.push('-no_rand_screen');

    function spawnClient() {
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
        server.close(common.mustCall(function() {
          setTimeout(callback, 100);
        }));
      }));
    }

    spawnClient();
  });

  process.on('exit', function() {
    // Each test run connects 6 times: an initial request and 5 reconnect
    // requests.
    assert.strictEqual(requestCount, 6);

    if (testOptions.tickets) {
      // No session cache callbacks are called.
      assert.strictEqual(resumeCount, 0);
      assert.strictEqual(newSessionCount, 0);
    } else if (testOptions.invalidSession) {
      // The resume callback was called, but each connection established a
      // fresh session.
      assert.strictEqual(resumeCount, 5);
      assert.strictEqual(newSessionCount, 6);
    } else {
      // The resume callback was called, and only the initial connection
      // establishes a fresh session.
      assert.ok(session);
      assert.strictEqual(resumeCount, 5);
      assert.strictEqual(newSessionCount, 1);
    }
  });
}
