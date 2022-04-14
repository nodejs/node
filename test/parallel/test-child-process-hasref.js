'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const command = common.isWindows ? 'dir' : 'ls';
const child = spawn(command);

assert.strictEqual(child.hasRef(), true);

child.unref();
assert.strictEqual(child.hasRef(), false);

child.ref();
assert.strictEqual(child.hasRef(), true);

function mustHasRef() {
  return common.mustCall(() => assert.strictEqual(child.hasRef(), true));
}

function mustHasNoRef() {
  return common.mustCall(() => assert.strictEqual(child.hasRef(), false));
}

child.stdout.on('data', mustHasRef());
child.stdout.on('end', mustHasRef());
child.stdout.on('close', mustHasNoRef());
child.on('exit', mustHasNoRef());
child.on('close', mustHasNoRef());
