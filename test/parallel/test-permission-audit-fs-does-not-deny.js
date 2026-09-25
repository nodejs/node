'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSync } = require('child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');

function runAudit(mode) {
  const childScript = `
    const dc = require('node:diagnostics_channel');
    const msgs = [];
    dc.subscribe('node:permission-model:fs', (m) => msgs.push({
      permission: m.permission,
      resource: m.resource,
    }));
    try {
      ${mode === 'eval' ?
        `eval('require("node:fs").readFileSync(process.env.BLOCKED_FILE)');` :
        `require('node:fs').readFileSync(process.env.BLOCKED_FILE);`}
      console.log('RESULT NO_THROW');
    } catch (e) {
      console.log('RESULT THREW ' + e.code);
    }
    console.log('AUDIT ' + JSON.stringify(msgs));
  `;

  const env = { ...process.env, BLOCKED_FILE: blockedFile };
  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    ['--permission-audit', '-e', childScript],
    { encoding: 'utf8', env },
  );
  assert.strictEqual(status, 0, stderr);
  const lines = stdout.split('\n');
  assert.ok(lines.includes('RESULT NO_THROW'), stdout);
  const auditLine = lines.find((l) => l.startsWith('AUDIT '));
  assert.ok(auditLine, stdout);
  const msgs = JSON.parse(auditLine.replace('AUDIT ', ''));
  assert.strictEqual(msgs.length, 1);
  assert.strictEqual(msgs[0].permission, 'FileSystemRead');
  assert.ok(msgs[0].resource.endsWith('protected-file.md'));
}

test('permission-audit logs fs denial without throwing', () => {
  runAudit('direct');
});

test('permission-audit logs fs denial without throwing (eval)', () => {
  runAudit('eval');
});
