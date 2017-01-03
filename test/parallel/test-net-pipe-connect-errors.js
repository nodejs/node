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
const fs = require('fs');
const net = require('net');
const path = require('path');
const assert = require('assert');

// Test if ENOTSOCK is fired when trying to connect to a file which is not
// a socket.

let emptyTxt;

if (common.isWindows) {
  // on Win, common.PIPE will be a named pipe, so we use an existing empty
  // file instead
  emptyTxt = path.join(common.fixturesDir, 'empty.txt');
} else {
  common.refreshTmpDir();
  // Keep the file name very short so tht we don't exceed the 108 char limit
  // on CI for a POSIX socket. Even though this isn't actually a socket file,
  // the error will be different from the one we are expecting if we exceed the
  // limit.
  emptyTxt = common.tmpDir + '0.txt';

  function cleanup() {
    try {
      fs.unlinkSync(emptyTxt);
    } catch (e) {
      assert.strictEqual(e.code, 'ENOENT');
    }
  }
  process.on('exit', cleanup);
  cleanup();
  fs.writeFileSync(emptyTxt, '');
}

const notSocketClient = net.createConnection(emptyTxt, function() {
  common.fail('connection callback should not run');
});

notSocketClient.on('error', common.mustCall(function(err) {
  assert(err.code === 'ENOTSOCK' || err.code === 'ECONNREFUSED',
         `received ${err.code} instead of ENOTSOCK or ECONNREFUSED`);
}));


// Trying to connect to not-existing socket should result in ENOENT error
const noEntSocketClient = net.createConnection('no-ent-file', function() {
  common.fail('connection to non-existent socket, callback should not run');
});

noEntSocketClient.on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, 'ENOENT');
}));


// On Windows or when running as root, a chmod has no effect on named pipes
if (!common.isWindows && process.getuid() !== 0) {
  // Trying to connect to a socket one has no access to should result in EACCES
  const accessServer = net.createServer(
    common.mustNotCall('server callback should not run'));
  accessServer.listen(common.PIPE, common.mustCall(function() {
    fs.chmodSync(common.PIPE, 0);

    const accessClient = net.createConnection(common.PIPE, function() {
      common.fail('connection should get EACCES, callback should not run');
    });

    accessClient.on('error', common.mustCall(function(err) {
      assert.strictEqual(err.code, 'EACCES');
      accessServer.close();
    }));
  }));
}
