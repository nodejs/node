// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const scenarios = [
  'constructors', 'assignment', 'operators', 'finished', 'pipeline',
  'compose', 'esm', 'vm',
];

// Each scenario starts with an untouched stream module, with and without
// the embedded snapshot that preloads Readable and Writable.
for (const flags of [[], ['--no-node-snapshot']]) {
  for (const scenario of scenarios) {
    const child = spawnSync(process.execPath, [
      ...process.execArgv, ...flags, fixtures.path('stream-lazy-load.js'), scenario,
    ], { encoding: 'utf8' });
    assert.strictEqual(child.status, 0, `${scenario}: ${child.stdout}\n${child.stderr}`);
  }
  const child = spawnSync(process.execPath, [
    ...process.execArgv, ...flags, fixtures.path('stream-lazy-frozen.js'),
  ], { encoding: 'utf8' });
  assert.strictEqual(child.status, 0, `frozen: ${child.stdout}\n${child.stderr}`);
}
