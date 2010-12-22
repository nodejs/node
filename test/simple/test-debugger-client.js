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

var expectedConnections = 0;
var tests = [];
function addTest (cb) {
  expectedConnections++;
  tests.push(cb);
}

addTest(function (client, done) {
  console.error("requesting version");
  client.reqVersion(function (v) {
    console.log("version: %s", v);
    assert.equal(process.versions.v8, v);
    done();
  });
});

addTest(function (client, done) {
  console.error("requesting scripts");
  client.reqScripts(function (s) {
    console.error("got %d scripts", s.length);
    var foundMainScript = false;
    for (var i = 0; i < s.length; i++) {
      if (s[i].name === 'node.js') {
        foundMainScript = true;
        break;
      }
    }
    assert.ok(foundMainScript);
    done();
  });
});

addTest(function (client, done) {
  console.error("eval 2+2");
  client.reqEval("2+2", function (res) {
    console.error(res);
    assert.equal('4', res.text);
    assert.equal(4, res.value);
    done();
  });
});


var connectCount = 0;

function doTest(cb, done) {
  var nodeProcess = spawn(process.execPath,
      ['-e', 'setInterval(function () { console.log("blah"); }, 100);']);

  nodeProcess.stdout.once('data', function () {
    console.log(">>> new node process: %d", nodeProcess.pid);
    process.kill(nodeProcess.pid, "SIGUSR1");
    console.log(">>> signaling it with SIGUSR1");
  });

  var didTryConnect = false;
  nodeProcess.stderr.setEncoding('utf8');
  nodeProcess.stderr.on('data', function (data) {
    if (didTryConnect == false && /debugger/.test(data)) {
      didTryConnect = true;

      // Wait for some data before trying to connect
      var c = new debug.Client();
      process.stdout.write(">>> connecting...");
      c.connect(debug.port)
      c.on('ready', function () {
        connectCount++;
        console.log("ready!");
        cb(c, function () {
          console.error(">>> killing node process %d\n\n", nodeProcess.pid);
          nodeProcess.kill();
          done();
        });
      });
    }
  });
}


function run () {
  var t = tests[0];
  if (!t) return;

  doTest(t, function () {
    tests.shift();
    run();
  });
}

run();

process.on('exit', function() {
  assert.equal(expectedConnections, connectCount);
});

