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
const exec = require('child_process').exec;
const tls = require('tls');
const fs = require('fs');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent2-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent2-cert.pem`),
  ciphers: 'AES256-SHA'
};

const reply = 'I AM THE WALRUS'; // something recognizable
let response = '';

process.on('exit', function() {
  assert.ok(response.includes(reply));
});

const server = tls.createServer(options, common.mustCall(function(conn) {
  conn.end(reply);
}));

server.listen(0, '127.0.0.1', function() {
  let cmd = `"${common.opensslCli}" s_client -cipher ${
    options.ciphers} -connect 127.0.0.1:${this.address().port}`;

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    cmd += ' -no_rand_screen';

  exec(cmd, function(err, stdout, stderr) {
    assert.ifError(err);
    response = stdout;
    server.close();
  });
});
