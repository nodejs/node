'use strict';
require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');
const fixture = fixtures.path('test-runner/throws_sync_and_async.js');

for (const isolation of ['none', 'process']) {
  const args = [
    '--test',
    '--test-reporter=spec',
    '--test-force-exit',
    `--test-isolation=${isolation}`,
    fixture,
  ];
  const r = spawnSync(process.execPath, args);

  assert.strictEqual(r.status, 1);
  assert.strictEqual(r.signal, null);
  assert.strictEqual(r.stderr.toString(), '');

  const stdout = r.stdout.toString();
  assert.match(stdout, /Error: fails/);
  assert.doesNotMatch(stdout, /this should not have a chance to be thrown/);
}
