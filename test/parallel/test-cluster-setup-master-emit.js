'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

assert(cluster.isMaster);

var assertsRun = 0;

function emitAndCatch(next) {
  cluster.once('setup', function(settings) {
    assert.strictEqual(settings.exec, 'new-exec');
    console.log('ok "setup" emitted with options set');
    assertsRun += 1;
    setImmediate(next);
  });
  cluster.setupMaster({ exec: 'new-exec' });
}

function emitAndCatch2(next) {
  cluster.once('setup', function(settings) {
    assert('exec' in settings);
    console.log('ok "setup" emitted without options set');
    assertsRun += 1;
    setImmediate(next);
  });
  cluster.setupMaster();
}

process.on('exit', function() {
  assert.strictEqual(assertsRun, 2);
  console.log('ok correct number of assertions');
});

emitAndCatch(function() {
  emitAndCatch2(function() {
    console.log('ok emitted and caught');
  });
});
