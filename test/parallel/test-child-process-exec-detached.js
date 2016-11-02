'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const execFile = require('child_process').execFile;
const childPath = require.resolve(common.fixturesDir + '/parent-process-exec-nonpersistent.js');

var persistentPid = -1;

const execParent = execFile(process.execPath, [ childPath], {
  timeout: 1000
});

execParent.stdout.on('data', function(data) {
  persistentPid = data;
});

process.on('exit', () => {
  assert.notStrictEqual(persistentPid, -1);
  assert.throws(function(data) {
    process.kill(execParent.pid);
  });
  assert.doesNotThrow(function() {
    process.kill(persistentPid);
  });
});
