var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var tests = [
  nextTick,
  timer,
  timerPlusNextTick,
  firstRun
]

tests.forEach(function(test) {
  console.log(test.name);
  var child = spawn(process.execPath, [
    '--abort-on-uncaught-exception',
    '-e',
    '(' + test+ ')()'
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
