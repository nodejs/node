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

const { opensslCli } = require('../common/crypto');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

if (!opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
}

const key = fixtures.readKey('rsa_private.pem');
const cert = fixtures.readKey('rsa_cert.crt');

doTest();

// This test consists of three TLS requests --
// * The first one should result in a new connection because we don't have
//   a valid session ticket.
// * The second one should result in connection resumption because we used
//   the session ticket we saved from the first connection.
// * The third one should result in a new connection because the ticket
//   that we used has expired by now.

function doTest() {
  const fs = require('fs');
  const spawn = require('child_process').spawn;

  const SESSION_TIMEOUT = 5;

  const options = {
    key: key,
    cert: cert,
    ca: [cert],
    sessionTimeout: SESSION_TIMEOUT,
    maxVersion: 'TLSv1.2',
    sessionIdContext: 'test-session-timeout',
  };

  const sessionFileName = tmpdir.resolve('tls-session-ticket.txt');
  // Expects a callback -- cb()
  function Client(port, sessIn, sessOut, expectedType, cb) {
    const flags = [
      's_client',
      '-connect', `localhost:${port}`,
      '-CAfile', fixtures.path('keys', 'rsa_cert.crt'),
      '-servername', 'localhost',
    ];
    if (sessIn) {
      flags.push('-sess_in', sessIn);
    }
    if (sessOut) {
      flags.push('-sess_out', sessOut);
    }
    const client = spawn(opensslCli, flags, {
      stdio: ['ignore', 'pipe', 'inherit']
    });

    let clientOutput = '';
    client.stdout.on('data', (data) => {
      clientOutput += data.toString();
    });
    client.on('exit', (code) => {
      let connectionType;
      // Log the output for debugging purposes. Don't remove them or otherwise
      // the CI output is useless when this test flakes.
      console.log(' ----- [COMMAND] ---');
      console.log(`${opensslCli}, ${flags.join(' ')}`);
      console.log(' ----- [STDOUT] ---');
      console.log(clientOutput);
      console.log(' ----- [SESSION FILE] ---');
      try {
        const stat = fs.statSync(sessionFileName);
        console.log(`Session file size: ${stat.size} bytes`);
      } catch (err) {
        console.log('Error reading session file:', err);
      }

      const grepConnectionType = (line) => {
        const matches = line.match(/(New|Reused), /);
        if (matches) {
          connectionType = matches[1];
          return true;
        }
      };
      const lines = clientOutput.split('\n');
      if (!lines.some(grepConnectionType)) {
        throw new Error('unexpected output from openssl client');
      }
      assert.strictEqual(code, 0);
      assert.strictEqual(connectionType, expectedType);
      cb(connectionType);
    });
  }

  const server = tls.createServer(options, (cleartext) => {
    cleartext.on('error', (er) => {
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    cleartext.end();
  });

  server.listen(0, () => {
    const port = server.address().port;
    Client(port, undefined, sessionFileName, 'New', () => {
      setTimeout(() => {
        Client(port, sessionFileName, sessionFileName, 'Reused', () => {
          setTimeout(() => {
            Client(port, sessionFileName, sessionFileName, 'New', () => {
              server.close();
            });
          }, (SESSION_TIMEOUT + 1) * 1000);
        });
      }, 100);  // Wait a bit to ensure the session ticket is saved.
    });
  });
}
