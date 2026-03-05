// Flags: --expose-internals
'use strict';

// Verify that the USDT dc__publish probe fires and provides the correct
// channel name by tracing a child Node.js process with bpftrace.

const common = require('../common');

if (!common.isLinux)
  common.skip('bpftrace tests are Linux-only');

const { internalBinding } = require('internal/test/binding');
const { probeSemaphore } = internalBinding('diagnostics_channel');
if (probeSemaphore === undefined)
  common.skip('Node.js built without USDT support');

const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

// bpftrace requires root.
if (process.getuid() !== 0)
  common.skip('bpftrace requires root privileges');

const bpftrace = spawnSync('bpftrace', ['--version']);
if (bpftrace.error)
  common.skip('bpftrace not found');

const fixtureScript = fixtures.path('diagnostics-channel-usdt-publish.js');

// bpftrace program: attach to the dc__publish probe, print the channel name,
// then exit after the traced process finishes.
const bpfProgram = `
usdt:${process.execPath}:node:dc__publish {
  printf("PROBE_FIRED channel=%s\\n", str(arg0));
}
`;

const result = spawnSync('bpftrace', [
  '-e', bpfProgram,
  '-c', `${process.execPath} ${fixtureScript}`,
], {
  timeout: 30_000,
  encoding: 'utf-8',
});

if (result.error)
  throw result.error;

if (result.status !== 0) {
  const stderr = result.stderr || '';
  // If bpftrace specifically cannot find our probe, that is a real failure
  // in the USDT implementation, not an environmental issue.
  if (stderr.includes('No probes found') ||
      stderr.includes('ERROR: usdt probe')) {
    assert.fail(`USDT probe broken - bpftrace could not attach: ${stderr}`);
  }
  // Otherwise bpftrace may fail for kernel/permission reasons unrelated
  // to our code.
  common.skip(`bpftrace exited with status ${result.status}: ${stderr}`);
}

const output = result.stdout;
assert.match(output, /PROBE_FIRED channel=test:usdt:bpftrace/,
             `Expected probe to fire with channel name. stdout: ${output}`);
