'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

const spawn = require('child_process').spawn;
const childPath = path.join(common.fixturesDir,
                            'parent-process-nonpersistent.js');
let persistentPid = -1;

const child = spawn(process.execPath, [ childPath ]);

child.stdout.on('data', function(data) {
  persistentPid = parseInt(data, 10);
});

process.on('exit', function() {
  assert.notStrictEqual(persistentPid, -1);
  assert.throws(function() {
    process.kill(child.pid);
  }, /^Error: kill ESRCH$/);
  assert.doesNotThrow(function() {
    process.kill(persistentPid);
  });
});
