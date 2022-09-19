'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;
const assert = require('assert');

const subprocess = spawn('echo', ['ok']);

let didSpawn = false;
subprocess.on('spawn', function() {
  didSpawn = true;
});
function mustCallAfterSpawn() {
  return common.mustCall(function() {
    assert.ok(didSpawn);
  });
}

subprocess.on('error', common.mustNotCall());
subprocess.on('spawn', common.mustCall());
subprocess.stdout.on('data', mustCallAfterSpawn());
subprocess.stdout.on('end', mustCallAfterSpawn());
subprocess.stdout.on('close', mustCallAfterSpawn());
subprocess.stderr.on('data', common.mustNotCall());
subprocess.stderr.on('end', mustCallAfterSpawn());
subprocess.stderr.on('close', mustCallAfterSpawn());
subprocess.on('exit', mustCallAfterSpawn());
subprocess.on('close', mustCallAfterSpawn());
