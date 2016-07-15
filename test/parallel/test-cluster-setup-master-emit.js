'use strict';
const common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

assert(cluster.isMaster);

function emitAndCatch(next) {
  cluster.once('setup', common.mustCall(function(settings) {
    assert.strictEqual(settings.exec, 'new-exec');
    setImmediate(next);
  }));
  cluster.setupMaster({ exec: 'new-exec' });
}

function emitAndCatch2(next) {
  cluster.once('setup', common.mustCall(function(settings) {
    assert('exec' in settings);
    setImmediate(next);
  }));
  cluster.setupMaster();
}

emitAndCatch(common.mustCall(function() {
  emitAndCatch2(common.mustCall(function() {}));
}));
