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

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

const assert = require('assert');
const join = require('path').join;
const fs = require('fs');
const spawn = require('child_process').spawn;
const https = require('https');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/agent.key`),
  cert: fs.readFileSync(`${common.fixturesDir}/agent.crt`),
  requestCert: true,
  rejectUnauthorized: false
};

const modulus = 'A6F44A9C25791431214F5C87AF9E040177A8BB89AC803F7E09BBC3A5519F' +
                '349CD9B9C40BE436D0AA823A94147E26C89248ADA2BE3DD4D34E8C289646' +
                '94B2047D217B4F1299371EA93A83C89AB9440724131E65F2B0161DE9560C' +
                'DE9C13455552B2F49CF0FB00D8D77532324913F6F80FF29D0A131D29DB06' +
                'AFF8BE191B7920DC2DAE1C26EA82A47847A10391EF3BF6AABB3CC40FF821' +
                '00B03A4F0FF1809278E4DDFDA7DE954ED56DC7AD9A47EEBC37D771A366FC' +
                '60A5BCB72373BEC180649B3EFA0E9092707210B41B90032BB18BC91F2046' +
                'EBDAF1191F4A4E26D71879C4C7867B62FCD508E8CE66E82D128A71E91580' +
                '9FCF44E8DE774067F1DE5D70B9C03687';

const CRLF = '\r\n';
const body = 'hello world\n';
let cert;

const server = https.createServer(options, common.mustCall(function(req, res) {
  console.log('got request');

  cert = req.connection.getPeerCertificate();

  assert.strictEqual(cert.subjectaltname, 'URI:http://example.com/#me');
  assert.strictEqual(cert.exponent, '0x10001');
  assert.strictEqual(cert.modulus, modulus);
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
}));

server.listen(0, function() {
  const args = ['s_client',
                '-quiet',
                '-connect', `127.0.0.1:${this.address().port}`,
                '-cert', join(common.fixturesDir, 'foafssl.crt'),
                '-key', join(common.fixturesDir, 'foafssl.key')];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  const client = spawn(common.opensslCli, args);

  client.stdout.on('data', function(data) {
    const message = data.toString();
    const contents = message.split(CRLF + CRLF).pop();
    assert.strictEqual(body, contents);
    server.close();
  });

  client.stdin.write('GET /\n\n');

  client.on('error', function(error) {
    throw error;
  });
});
