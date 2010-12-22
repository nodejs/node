var common = require('../common');
var assert = require('assert');
var debug = require('_debugger');

var spawn = require('child_process').spawn;


var resCount = 0;
var p = new debug.Protocol();
p.onResponse = function (res) {
  resCount++;
};

p.execute("Type: connect\r\n" +
          "V8-Version: 3.0.4.1\r\n" +
          "Protocol-Version: 1\r\n" +
          "Embedding-Host: node v0.3.3-pre\r\n" +
          "Content-Length: 0\r\n\r\n");
assert.equal(1, resCount);


var connectCount = 0;

function test(cb) {
  var nodeProcess = spawn(process.execPath,
      ['-e', 'setInterval(function () { console.log("blah"); }, 1000);']);

  nodeProcess.stdout.once('data', function () {
    console.log("new node process: %d", nodeProcess.pid);
    process.kill(nodeProcess.pid, "SIGUSR1");
    console.log("signaling it with SIGUSR1");
  });

  var didTryConnect = false;
  nodeProcess.stderr.setEncoding('utf8');
  nodeProcess.stderr.on('data', function (data) {
    if (didTryConnect == false && /debugger/.test(data)) {
      didTryConnect = true;

      // Wait for some data before trying to connect
      var c = new debug.Client();
      process.stdout.write("connecting...");
      c.connect(debug.port, function () {
        connectCount++;
        console.log("connected!");
        cb(c, nodeProcess);
      });
    }
  });
}


test(function (client, nodeProcess) {
  client.reqVersion(function (v) {
    console.log("version: %s", v);
    assert.equal(process.versions.v8, v);
    nodeProcess.kill();
  });
});


process.on('exit', function() {
  assert.equal(1, connectCount);
});

