'use strict';
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

var emptyTxt;

if (common.isWindows) {
  // on Win, common.PIPE will be a named pipe, so we use an existing empty
  // file instead
  emptyTxt = path.join(common.fixturesDir, 'empty.txt');
} else {
  // use common.PIPE to ensure we stay within POSIX socket path length
  // restrictions, even on CI
  common.refreshTmpDir();
  emptyTxt = common.PIPE + '.txt';

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
  assert.ok(notSocketErrorFired);
  assert.ok(noEntErrorFired);
  if (!common.isWindows && process.getuid() !== 0) {
    assert.ok(accessErrorFired);
  }
});

