'use strict';

const common = require('../common');
if (!common.isLinux) {
  common.skip('thread names are read from /proc/self/task');
}

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');

function v8WorkerCount() {
  return fs.readdirSync('/proc/self/task').reduce((count, tid) => {
    try {
      const name = fs.readFileSync(`/proc/self/task/${tid}/comm`, 'utf8').trim();
      return count + (name === 'node-V8Worker' ? 1 : 0);
    } catch {
      return count;
    }
  }, 0);
}

// The default --v8-pool-size is 1, and workers are created at platform init.
assert.strictEqual(v8WorkerCount(), 1);

const script = `
  const fs = require('fs');
  const n = fs.readdirSync('/proc/self/task').reduce((count, tid) => {
    try {
      const comm = '/proc/self/task/' + tid + '/comm';
      const name = fs.readFileSync(comm, 'utf8').trim();
      return count + (name === 'node-V8Worker' ? 1 : 0);
    } catch {
      return count;
    }
  }, 0);
  process.stdout.write(String(n));
`;

const child = spawnSync(process.execPath, ['--v8-pool-size=4', '-e', script], {
  encoding: 'utf8',
});
assert.ifError(child.error);
assert.strictEqual(child.status, 0, child.stderr);
assert.strictEqual(child.stdout, '4');
