'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSync } = require('child_process');
const { test } = require('node:test');
const net = require('net');

async function runAudit(mode) {
  const server = net.createServer();

  await new Promise((resolve, reject) => {
    server.on('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const { port } = server.address();
  const env = { ...process.env, PORT: String(port), HOST: '127.0.0.1' };

  const childScript = `
    const dc = require('node:diagnostics_channel');
    const net = require('node:net');
    const msgs = [];
    dc.subscribe('node:permission-model:net', (m) => msgs.push({
      permission: m.permission,
      resource: m.resource,
    }));
    const s = ${mode === 'eval' ?
      `eval('net.connect(Number(process.env.PORT), process.env.HOST)')` :
      `net.connect(Number(process.env.PORT), process.env.HOST)`};
    s.on('connect', () => { s.destroy(); console.log('RESULT CONNECTED'); });
    s.on('error', (e) => { console.log('RESULT ERROR ' + e.code); });
    s.on('close', () => {
      console.log('AUDIT ' + JSON.stringify(msgs));
    });
  `;

  try {
    const { status, stdout, stderr } = spawnSync(
      process.execPath,
      ['--permission-audit', '-e', childScript],
      { encoding: 'utf8', env },
    );
    assert.strictEqual(status, 0, stderr);
    const lines = stdout.split('\n');
    assert.strictEqual(lines[0], 'RESULT CONNECTED', stdout);
    const auditLine = lines.find((l) => l.startsWith('AUDIT '));
    assert.ok(auditLine, stdout);
    const msgs = JSON.parse(auditLine.replace('AUDIT ', ''));
    assert.strictEqual(msgs.length, 1);
    assert.strictEqual(msgs[0].permission, 'Net');
    assert.ok(msgs[0].resource.includes('127.0.0.1'));
  } finally {
    server.close();
  }
}

test('permission-audit logs net denial without blocking connect', async () => {
  await runAudit('direct');
});

test('permission-audit logs net denial without blocking connect (eval)', async () => {
  await runAudit('eval');
});
