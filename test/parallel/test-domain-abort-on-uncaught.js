// Flags: --abort_on_uncaught_exception

var common = require('../common');
var assert = require('assert');
var domain = require('domain');

var tests = [
  nextTick,
  timer,
  timerPlusNextTick,
  netServer,
  firstRun,
];

var errors = 0;

process.on('exit', function() {
  assert.equal(errors, tests.length);
});

tests.forEach(function(test) { test(); })

function nextTick() {
  var d = domain.create();

  d.once('error', function(err) {
    errors += 1;
  });
  d.run(function() {
    process.nextTick(function() {
      throw new Error('exceptional!');
    });
  });
}

function timer() {
  var d = domain.create();

  d.on('error', function(err) {
    errors += 1;
  });
  d.run(function() {
    setTimeout(function() {
      throw new Error('exceptional!');
    }, 33);
  });
}

function timerPlusNextTick() {
  var d = domain.create();

  d.on('error', function(err) {
    errors += 1;
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
  var d = domain.create();

  d.on('error', function(err) {
    errors += 1;
  });
  d.run(function() {
    throw new Error('exceptional!');
  });
}

function netServer() {
  var net = require('net');
  var d = domain.create();

  d.on('error', function(err) {
    errors += 1;
  });
  d.run(function() {
    var server = net.createServer(function(conn) {
      conn.pipe(conn);
    });
    server.listen(common.PORT, '0.0.0.0', function() {
      var conn = net.connect(common.PORT, '0.0.0.0')
      conn.once('data', function() {
        throw new Error('ok');
      })
      conn.end('ok');
      server.close();
    });
  });
}
