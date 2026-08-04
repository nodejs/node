'use strict';

// Benchmarks startup time when deserialize callbacks are registered in a
// startup snapshot, measuring real binary execution with --snapshot-blob.

const common = require('../common.js');
const { spawnSync } = require('child_process');
const { mkdtempSync, writeFileSync, rmSync } = require('fs');
const { tmpdir } = require('os');
const path = require('path');

const bench = common.createBenchmark(main, {
  count: [10, 100, 1000, 10000],
  n: [30],
});

function buildSnapshot(snapshotBlob, fixtureScript, count) {
  writeFileSync(fixtureScript, `
'use strict';
const v8 = require('node:v8');
for (let i = 0; i < ${count}; i++) {
  v8.startupSnapshot.addDeserializeCallback(() => {});
}
v8.startupSnapshot.setDeserializeMainFunction(() => {});
`);
  const result = spawnSync(process.execPath, [
    '--snapshot-blob', snapshotBlob,
    '--build-snapshot', fixtureScript,
  ]);
  if (result.status !== 0) {
    throw new Error(`Failed to build snapshot:\n${result.stderr}`);
  }
}

function main({ n, count }) {
  const dir = mkdtempSync(path.join(tmpdir(), 'node-bench-snapshot-'));
  const snapshotBlob = path.join(dir, 'snapshot.blob');
  const fixtureScript = path.join(dir, 'build.js');

  try {
    buildSnapshot(snapshotBlob, fixtureScript, count);

    const warmup = 3;
    let finished = -warmup;

    while (finished < n) {
      const child = spawnSync(process.execPath, ['--snapshot-blob', snapshotBlob]);
      if (child.status !== 0) {
        throw new Error(`Snapshot run failed:\n${child.stderr}`);
      }
      finished++;
      if (finished === 0) bench.start();
      if (finished === n) bench.end(n);
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}
