'use strict';

require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');


const help = cp.spawnSync(process.execPath, ['-h']);

const manualPath = path.join(__dirname, '..', '..', 'doc', 'node.1');
const man = cp.spawnSync('man', [manualPath]);

if (man.status === 0) {
  assert.strictEqual(help.stdout.toString(), man.stdout.toString());
} else {
  const raw = cp.spawnSync(process.execPath, ['--raw-help']);
  assert.strictEqual(help.stdout.toString(), raw.stdout.toString());
}
