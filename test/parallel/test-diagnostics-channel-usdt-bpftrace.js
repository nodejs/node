// Flags: --expose-internals
'use strict';

// Verify that the USDT dc__publish probe fires and provides the correct
// channel name by tracing child Node.js processes with bpftrace, including
// after a startup snapshot is built (a publish during --build-snapshot must
// not leave probe state that keeps probes disabled after restore).

const common = require('../common');

if (!common.isLinux)
  common.skip('bpftrace tests are Linux-only');

const { internalBinding } = require('internal/test/binding');
const { probeSemaphore } = internalBinding('diagnostics_channel');
if (probeSemaphore === undefined)
  common.skip('Node.js built without USDT support');

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

// The bpftrace tool requires root to attach uprobes.
if (process.getuid() !== 0)
  common.skip('bpftrace requires root privileges');

const bpftrace = spawnSync('bpftrace', ['--version']);
if (bpftrace.error)
  common.skip('bpftrace not found');

// The bpftrace program: attach to the dc__publish probe, print the channel
// name, then exit after the traced process finishes.
const bpfProgram = `
usdt:${process.execPath}:node:dc__publish {
  printf("PROBE_FIRED channel=%s\\n", str(arg0));
}
`;

// Run `command` under bpftrace and return the spawn result. If bpftrace
// cannot find the USDT probe that is a failure of the USDT implementation
// itself; other bpftrace failures are treated as environmental.
function runBpftrace(command) {
  const result = spawnSync('bpftrace', ['-e', bpfProgram, '-c', command], {
    timeout: 30_000,
    encoding: 'utf-8',
  });

  assert.ifError(result.error);

  if (result.status !== 0) {
    const stderr = result.stderr || '';
    // If bpftrace specifically cannot find our probe, that is a real
    // failure in the USDT implementation, not an environmental issue.
    if (stderr.includes('No probes found') ||
        stderr.includes('ERROR: usdt probe')) {
      assert.fail(`USDT probe broken - bpftrace could not attach: ${stderr}`);
    }
    // Otherwise bpftrace may fail for kernel/permission reasons unrelated
    // to our code.
    common.skip(`bpftrace exited with status ${result.status}: ${stderr}`);
  }

  return result;
}

// Scenario 1: a plain publish is observable by a tracer.
{
  const fixtureScript = fixtures.path('diagnostics-channel-usdt-publish.js');

  const result = runBpftrace(`${process.execPath} ${fixtureScript}`);
  assert.match(result.stdout, /PROBE_FIRED channel=test:usdt:bpftrace/,
               `Expected probe to fire with channel name. stdout: ${result.stdout}`);
}

// Scenario 2: publish during --build-snapshot, then restore the snapshot
// and publish again under a tracer. This covers the case where probe state
// captured while building a startup snapshot goes stale on deserialization.
{
  tmpdir.refresh();
  const blobPath = tmpdir.resolve('usdt-snapshot.blob');
  const buildScript = tmpdir.resolve('usdt-snapshot-build.js');
  const runScript = tmpdir.resolve('usdt-snapshot-run.js');
  const childSource = `
'use strict';
const dc = require('node:diagnostics_channel');
const ch = dc.channel('test:usdt:snapshot');
ch.subscribe(() => {});
for (let i = 0; i < 10; i++) {
  ch.publish({ seq: i });
}
`;
  fs.writeFileSync(buildScript, childSource);
  fs.writeFileSync(runScript, childSource);

  const build = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    buildScript,
  ], {
    timeout: 30_000,
    encoding: 'utf-8',
  });
  assert.ifError(build.error);
  assert.strictEqual(build.status, 0,
                     `snapshot build failed: ${build.stderr}`);

  const result = runBpftrace(
    `${process.execPath} --snapshot-blob=${blobPath} ${runScript}`);
  assert.match(result.stdout, /PROBE_FIRED channel=test:usdt:snapshot/,
               `Expected probe to fire after snapshot restore. stdout: ${result.stdout}`);
}
