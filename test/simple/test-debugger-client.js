var common = require('../common');
var assert = require('assert');
var d = require('_debugger');

var spawn = require('child_process').spawn;


var resCount = 0;
var p = new d.Protocol();
p.onResponse = function (res) {
  resCount++;
};

p.execute("Type: connect\r\n" +
          "V8-Version: 3.0.4.1\r\n" +
          "Protocol-Version: 1\r\n" +
          "Embedding-Host: node v0.3.3-pre\r\n" +
          "Content-Length: 0\r\n\r\n");
assert.equal(1, resCount);

var n = spawn(process.execPath,
              ['-e', 'setInterval(function () { console.log("blah"); }, 1000);']);


var connected = false;

n.stdout.once('data', function () {
  console.log("new node process: %d", n.pid);
  process.kill(n.pid, "SIGUSR1");
  console.log("signaling it with SIGUSR1");

});

var didTryConnect = false;
n.stderr.setEncoding('utf8');
n.stderr.on('data', function (d) {
  if (didTryConnect == false && /debugger/.test(d)) {
    didTryConnect = true;
    tryConnect();
  }
})


function tryConnect() {
  // Wait for some data before trying to connect
  var c = new d.Client();
  process.stdout.write("connecting...");
  c.connect(d.port, function () {
    connected = true;
    console.log("connected!");
  });

  c.reqVersion(function (v) {
    assert.equal(process.versions.v8, v);
    n.kill();
  });
}


process.on('exit', function() {
  assert.ok(connected);
});

