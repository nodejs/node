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




// Test sending and receiving a file descriptor.
//
// This test is pretty complex. It ends up spawning test/fixtures/recvfd.js
// as a child to test desired behavior. What happens is
//
//  1. Create an in-memory pipe via pipe(2). These two file descriptors
//     are not visible to any other process, and so make a good test-case
//     for sharing.
//  2. Create a a UNIX socket at SOCK_PATH. When a client connects to this
//     path, they are sent the write end of the pipe from above.
//  3. The client is sent n JSON representations of the DATA variable, each
//     with a different ordinal. We send these delimited by '\n' strings
//     so that the receiving end can avoid any coalescing that hapepns
//     due to the stream nature of the socket (e.g. '{}{}' is not a valid
//     JSON string).
//  4. The child process receives file descriptors and JSON blobs and,
//     whenever it has at least one of each, writes a modified JSON blob
//     to the FD. The blob is modified to include the child's process ID.
//  5. Once the child process has sent n responses, it closes the write end
//     of the pipe, which signals to the parent that there is no more data
//     coming.
//  6. The parent listens to the read end of the pipe, accumulating JSON
//     blobs (again, delimited by '\n') and verifying that a) the 'pid'
//     attribute belongs to the child and b) the 'ord' field has not been
//     seen in a response yet. This is intended to ensure that all blobs
//     sent out have been relayed back to us.

var common = require('../common');
var assert = require('assert');

var buffer = require('buffer');
var child_process = require('child_process');
var fs = require('fs');
var net = require('net');
var netBinding = process.binding('net');
var path = require('path');

var DATA = {
  'ppid' : process.pid,
  'ord' : 0
};

var SOCK_PATH = path.join(__dirname,
                          '..',
                          path.basename(__filename, '.js') + '.sock');

var logChild = function(d) {
  if (typeof d == 'object') {
    d = d.toString();
  }

  d.split('\n').forEach(function(l) {
    if (l.length > 0) {
      common.debug('CHILD: ' + l);
    }
  });
};

// Create a pipe
//
// We establish a listener on the read end of the pipe so that we can
// validate any data sent back by the child. We send the write end of the
// pipe to the child and close it off in our process.
var pipeFDs = netBinding.pipe();
assert.equal(pipeFDs.length, 2);

var seenOrdinals = [];

var pipeReadStream = new net.Stream();
pipeReadStream.on('data', function(data) {
  data.toString('utf8').trim().split('\n').forEach(function(d) {
    var rd = JSON.parse(d);

    assert.equal(rd.pid, cpp);
    assert.equal(seenOrdinals.indexOf(rd.ord), -1);

    seenOrdinals.unshift(rd.ord);
  });
});
pipeReadStream.open(pipeFDs[0]);
pipeReadStream.resume();

// Create a UNIX socket at SOCK_PATH and send DATA and the write end
// of the pipe to whoever connects.
//
// We send two messages here, both with the same pipe FD: one string, and
// one buffer. We want to make sure that both datatypes are handled
// correctly.
var srv = net.createServer(function(s) {
  var str = JSON.stringify(DATA) + '\n';

  DATA.ord = DATA.ord + 1;
  var buf = new buffer.Buffer(str.length);
  buf.write(JSON.stringify(DATA) + '\n', 'utf8');

  s.write(str, 'utf8', pipeFDs[1]);
  if (s.write(buf, pipeFDs[1])) {
    netBinding.close(pipeFDs[1]);
  } else {
    s.on('drain', function() {
      netBinding.close(pipeFDs[1]);
    });
  }
});
srv.listen(SOCK_PATH);

// Spawn a child running test/fixtures/recvfd.js
var cp = child_process.spawn(process.argv[0],
                             [path.join(common.fixturesDir, 'recvfd.js'),
                              SOCK_PATH]);

cp.stdout.on('data', logChild);
cp.stderr.on('data', logChild);

// When the child exits, clean up and validate its exit status
var cpp = cp.pid;
cp.on('exit', function(code, signal) {
  srv.close();
  // fs.unlinkSync(SOCK_PATH);

  assert.equal(code, 0);
  assert.equal(seenOrdinals.length, 2);
});

// vim:ts=2 sw=2 et
