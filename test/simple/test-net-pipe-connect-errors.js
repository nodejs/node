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

var fs = require('fs');
var net = require('net');
var path = require('path');
var assert = require('assert');
var common = require('../common');

var notSocketErrorFired = false;
var noEntErrorFired = false;
var accessErrorFired = false;

// Test if ENOTSOCK is fired when trying to connect to a file which is not
// a socket.
var emptyTxt = path.join(common.fixturesDir, 'empty.txt');
var notSocketClient = net.createConnection(emptyTxt, function() {
  assert.ok(false);
});

notSocketClient.on('error', function(err) {
  assert(err.code === 'ENOTSOCK' || err.code === 'ECONNREFUSED');
  notSocketErrorFired = true;
});


// Trying to connect to not-existing socket should result in ENOENT error
var noEntSocketClient = net.createConnection('no-ent-file', function() {
  assert.ok(false);
});

noEntSocketClient.on('error', function(err) {
  assert.equal(err.code, 'ENOENT');
  noEntErrorFired = true;
});


// Trying to connect to a socket one has no access to should result in EACCES
var accessServer = net.createServer(function() {
  assert.ok(false);
});
accessServer.listen(common.PIPE, function() {
  fs.chmodSync(common.PIPE, 0);

  var accessClient = net.createConnection(common.PIPE, function() {
    assert.ok(false);
  });

  accessClient.on('error', function(err) {
    assert.equal(err.code, 'EACCES');
    accessErrorFired = true;
    accessServer.close();
  });
});


// Assert that all error events were fired
process.on('exit', function() {
  assert.ok(notSocketErrorFired);
  assert.ok(noEntErrorFired);
  assert.ok(accessErrorFired);
});

