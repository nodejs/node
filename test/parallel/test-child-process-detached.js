'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');

var spawn = require('child_process').spawn;
var childPath = path.join(common.fixturesDir,
                          'parent-process-nonpersistent.js');
var persistentPid = -1;

var child = spawn(process.execPath, [ childPath ]);

child.stdout.on('data', function(data) {
  persistentPid = parseInt(data, 10);
});

process.on('exit', function() {
  assert(persistentPid !== -1);
  assert.throws(function() {
    process.kill(child.pid);
  });
  assert.doesNotThrow(function() {
    process.kill(persistentPid);
  });
});

