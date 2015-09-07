'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');
var spawn = require('child_process').spawn;
var cluster = require('cluster');

if (common.isWindows) {
  console.log("1..0 # Skipped: passing UDP handles isn't supported on Windows");
  return;
}

var isDuplicateMaster = (process.argv[3] === 'dupmaster');
var isDuplicateFork = (process.argv[3] === 'dupfork');
var port = (isDuplicateMaster || isDuplicateFork ? +process.argv[2] : 0);

if (cluster.isMaster) {
  if (isDuplicateMaster)
    cluster.setupMaster({ args: [port, 'dupfork'] });
  var worker = cluster.fork();
  if (isDuplicateMaster) {
    worker.once('message', common.mustCall(function(msg) {
      assert.strictEqual(msg.portno, port);
    }));
  }
  worker.on('exit', common.mustCall(function(exitCode) {
    assert.strictEqual(exitCode, 0);
    if (isDuplicateMaster)
      process.stdout.write(''+port);
  }));
} else {
  var socket = dgram.createSocket({type: 'udp4'/*, reuseAddr: true*/});
  socket.bind(port, common.localhostIPv4, common.mustCall(function() {
    port = socket.address().port;

    if (isDuplicateFork) {
      process.send({ portno: port });
      return setImmediate(cluster.worker.disconnect.bind(cluster.worker));
    }

    // Start duplicate cluster that listens on the same port ...
    var child = spawn(process.execPath,
                      [__filename, port, 'dupmaster'],
                      {env: {}});
    var stdoutBuf = '';
    child.stdout.on('data', function(data) {
      stdoutBuf += data;
    });
    child.stderr.pipe(process.stderr);
    child.on('close', common.mustCall(function(code, signal) {
      assert.strictEqual(stdoutBuf, ''+port);
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
      cluster.worker.disconnect();
    }));
  }));
}
