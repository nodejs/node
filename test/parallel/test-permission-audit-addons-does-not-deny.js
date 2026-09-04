'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

// This test ensures that --permission-audit does not disable native addon
// loading. In audit mode process.dlopen() must reach the regular loading
// path (failing with ERR_DLOPEN_FAILED for a missing file, after publishing
// the denial to the diagnostics channel) instead of being rejected upfront
// with ERR_DLOPEN_DISABLED, which is still the expected behavior under
// --permission.

const assert = require('assert');
const { spawnSync } = require('child_process');
const { test } = require('node:test');

function run(flag) {
  const childScript = `
    const dc = require('node:diagnostics_channel');
    const msgs = [];
    dc.subscribe('node:permission-model:addon', (m) => msgs.push({
      permission: m.permission,
      resource: m.resource,
    }));
    try {
      process.dlopen({ exports: {} }, '/nonexistent/audit-test.node');
      console.log('RESULT NO_THROW');
    } catch (e) {
      console.log('RESULT THREW ' + e.code);
    }
    console.log('AUDIT ' + JSON.stringify(msgs));
  `;

  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    [flag, '-e', childScript],
    { encoding: 'utf8' },
  );
  assert.strictEqual(status, 0, stderr);
  const lines = stdout.split('\n');
  const resultLine = lines.find((l) => l.startsWith('RESULT '));
  assert.ok(resultLine, stdout);
  const auditLine = lines.find((l) => l.startsWith('AUDIT '));
  assert.ok(auditLine, stdout);
  return {
    result: resultLine.replace('RESULT ', ''),
    msgs: JSON.parse(auditLine.replace('AUDIT ', '')),
  };
}

test('audit mode reaches the real dlopen and logs the denial', () => {
  const { result, msgs } = run('--permission-audit');
  assert.strictEqual(result, 'THREW ERR_DLOPEN_FAILED');
  assert.strictEqual(msgs.length, 1);
  assert.strictEqual(msgs[0].permission, 'Addon');
});

test('enforce mode still disables dlopen upfront', () => {
  const { result } = run('--permission');
  assert.strictEqual(result, 'THREW ERR_DLOPEN_DISABLED');
});
