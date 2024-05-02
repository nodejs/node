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
// Server sends a large string. Client counts bytes and pauses every few
// seconds. Makes sure that pause and resume work properly.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

process.stdout.write('build body...');
const body = 'hello world\n'.repeat(1024 * 1024);
process.stdout.write('done\n');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};

const server = tls.Server(options, common.mustCall(function(socket) {
  socket.end(body);
}));

let recvCount = 0;

server.listen(0, function() {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  });

  client.on('data', function(d) {
    process.stdout.write('.');
    recvCount += d.length;

    client.pause();
    process.nextTick(function() {
      client.resume();
    });
  });


  client.on('close', function() {
    console.error('close');
    server.close();
    clearTimeout(timeout);
  });
});


function displayCounts() {
  console.log(`body.length: ${body.length}`);
  console.log(`  recvCount: ${recvCount}`);
}


const timeout = setTimeout(displayCounts, 10 * 1000);


process.on('exit', function() {
  displayCounts();
  assert.strictEqual(body.length, recvCount);
});
