'use strict';

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');

// When the test fails this helper can be modified to write outputs
// differently and aid debugging.
function log(line) {
  console.log(line);
}

function generateSnapshot() {
  tmpdir.refresh();

  spawnSyncAndAssert(
    process.execPath,
    [
      '--random_seed=42',
      '--predictable',
      '--build-snapshot',
      'node:generate_default_snapshot_source',
    ],
    {
      env: { ...process.env, NODE_DEBUG_NATIVE: 'SNAPSHOT_SERDES' },
      cwd: tmpdir.path
    },
    {
      stderr(output) {
        const lines = output.split('\n');
        for (const line of lines) {
          if (line.startsWith('0x')) {
            log(line);
          }
        }
      },
    }
  );
  const outputPath = tmpdir.resolve('snapshot.cc');
  return fs.readFileSync(outputPath, 'utf-8').split('\n');
}

const source1 = generateSnapshot();
const source2 = generateSnapshot();
assert.deepStrictEqual(source1, source2);
