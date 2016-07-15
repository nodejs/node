'use strict';
var common = require('../common');
var fs = require('fs');
var net = require('net');
var path = require('path');
var assert = require('assert');

var accessErrorFired = false;

// Test if ENOTSOCK is fired when trying to connect to a file which is not
// a socket.

var emptyTxt;

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
      if (e.code != 'ENOENT')
        throw e;
    }
  }
  process.on('exit', cleanup);
  cleanup();
  fs.writeFileSync(emptyTxt, '');
}

var notSocketClient = net.createConnection(emptyTxt, function() {
  assert.ok(false);
});

notSocketClient.on('error', common.mustCall(function(err) {
  assert(err.code === 'ENOTSOCK' || err.code === 'ECONNREFUSED',
         `received ${err.code} instead of ENOTSOCK or ECONNREFUSED`);
}));


// Trying to connect to not-existing socket should result in ENOENT error
var noEntSocketClient = net.createConnection('no-ent-file', function() {
  assert.ok(false);
});

noEntSocketClient.on('error', common.mustCall(function(err) {
  assert.equal(err.code, 'ENOENT');
}));


// On Windows or when running as root, a chmod has no effect on named pipes
if (!common.isWindows && process.getuid() !== 0) {
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
}


// Assert that all error events were fired
process.on('exit', function() {
  if (!common.isWindows && process.getuid() !== 0) {
    assert.ok(accessErrorFired);
  }
});
