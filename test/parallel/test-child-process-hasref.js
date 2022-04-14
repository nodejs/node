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

child.on(
  'exit',
  common.mustCall(() => assert.strictEqual(child.hasRef(), false)),
);

child.on(
  'close',
  common.mustCall(() => assert.strictEqual(child.hasRef(), false)),
);
