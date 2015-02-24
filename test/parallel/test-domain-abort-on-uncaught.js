var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var tests = [
  nextTick,
  timer,
  timerPlusNextTick,
  firstRun,
  netServer
]

tests.forEach(function(test) {
  console.log(test.name);
  var child = spawn(process.execPath, [
    '--abort-on-uncaught-exception',
    '-e',
    '(' + test + ')()',
    common.PORT
  ]);
  child.stderr.pipe(process.stderr);
  child.stdout.pipe(process.stdout);
  child.on('exit', function(code) {
    assert.strictEqual(code, 0);
  });
});

function nextTick() {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function(err) {
    console.log('ok');
    process.exit(0);
  });
  d.run(function() {
    process.nextTick(function() {
      throw new Error('exceptional!');
    });
  });
}

function timer() {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function(err) {
    console.log('ok');
    process.exit(0);
  });
  d.run(function() {
    setTimeout(function() {
      throw new Error('exceptional!');
    }, 33);
  });
}

function timerPlusNextTick() {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function(err) {
    console.log('ok');
    process.exit(0);
  });
  d.run(function() {
    setTimeout(function() {
      process.nextTick(function() {
        throw new Error('exceptional!');
      });
    }, 33);
  });
}

function firstRun() {
  var domain = require('domain');
  var d = domain.create();

  d.on('error', function(err) {
    console.log('ok');
    process.exit(0);
  });
  d.run(function() {
    throw new Error('exceptional!');
  });
}

function netServer() {
  var domain = require('domain');
  var net = require('net');
  var d = domain.create();

  d.on('error', function(err) {
    console.log('ok');
    process.exit(0);
  });
  d.run(function() {
    var server = net.createServer(function(conn) {
      conn.pipe(conn);
    });
    server.listen(Number(process.argv[1]), '0.0.0.0', function() {
      var conn = net.connect(Number(process.argv[1]), '0.0.0.0')
      conn.once('data', function() {
        throw new Error('ok');
      })
      conn.end('ok');
    });
  });
}
