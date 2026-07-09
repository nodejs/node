'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { isMainThread } = require('node:worker_threads');
const { test } = require('node:test');
const fs = require('node:fs');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const restrictedFile = tmpdir.resolve('restricted.txt');

const childScript = `
  const dc = require('node:diagnostics_channel');
  const msgs = [];
  dc.subscribe('node:permission-model:fs', (m) => msgs.push({
    permission: m.permission,
    resource: m.resource,
  }));
  try {
    require('node:fs').readFileSync(${JSON.stringify(restrictedFile)});
    console.log('NO_THROW');
  } catch (e) {
    console.log('THREW ' + e.code);
  }
  console.log('AUDIT ' + JSON.stringify(msgs));
`;

test('permission-audit logs fs denial without throwing', () => {
  tmpdir.refresh();
  fs.writeFileSync(restrictedFile, 'x');

  // Audit mode: should NOT throw, SHOULD publish the channel message.
  {
    const { stdout, stderr, status } = spawnSync(
      process.execPath,
      ['--permission-audit', '-e', childScript],
      { encoding: 'utf8' },
    );
    assert.strictEqual(status, 0, stderr);
    const lines = stdout.split('\n');
    assert.ok(lines.includes('NO_THROW'), stdout);
    const auditLine = lines.find((l) => l.startsWith('AUDIT'));
    assert.ok(auditLine, stdout);
    const msgs = JSON.parse(auditLine.replace('AUDIT ', ''));
    assert.ok(
      msgs.some((m) =>
        m.permission === 'FileSystemRead' &&
        String(m.resource).endsWith('restricted.txt')),
      JSON.stringify(msgs));
  }

  // Enforce mode (--permission): SHOULD throw ERR_ACCESS_DENIED (the child
  // script catches it, so the process exits 0 while logging THREW ...).
  {
    const { stdout, stderr, status } = spawnSync(
      process.execPath,
      ['--permission', '-e', childScript],
      { encoding: 'utf8' },
    );
    assert.strictEqual(status, 0, stderr);
    const lines = stdout.split('\n');
    assert.ok(lines.some((l) => l.startsWith('THREW ERR_ACCESS_DENIED')), stdout);
    assert.ok(!lines.includes('NO_THROW'), stdout);
  }
});
