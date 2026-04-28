'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');

common.skipIfInspectorDisabled();

tmpdir.refresh();
const intervals = 40;
// Outputs coverage when v8.takeCoverage() is invoked.
{
  const output = spawnSync(process.execPath, [
    '-r',
    fixtures.path('v8-coverage', 'take-coverage'),
    fixtures.path('v8-coverage', 'interval'),
  ], {
    env: {
      ...process.env,
      NODE_V8_COVERAGE: tmpdir.path,
      NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER',
      TEST_INTERVALS: intervals
    },
  });
  console.log(output.stderr.toString());
  assert.strictEqual(output.status, 0);
  const coverageFiles = fs.readdirSync(tmpdir.path);

  let coverages = [];
  for (const coverageFile of coverageFiles) {
    const coverage = require(tmpdir.resolve(coverageFile));
    for (const result of coverage.result) {
      if (result.url.includes('/interval')) {
        coverages.push({
          file: coverageFile,
          func: result.functions.find((f) => f.functionName === 'interval'),
          timestamp: coverage.timestamp
        });
      }
    }
  }

  coverages = coverages.sort((a, b) => { return a.timestamp - b.timestamp; });
  // There should be two coverages taken, one triggered by v8.takeCoverage(),
  // the other by process exit.
  console.log('Coverages:', coverages);
  assert.strictEqual(coverages.length, 3);

  let blockHitsTotal = 0;
  for (let i = 0; i < coverages.length; ++i) {
    const { ranges } = coverages[i].func;
    console.log('coverage', i, ranges);

    if (i !== coverages.length - 1) {
      // When the first two coverages are taken:
      assert.strictEqual(ranges.length, 2);
      const blockHits = ranges[0].count;
      // The block inside interval() should be hit at least once.
      assert.notStrictEqual(blockHits, 0);
      blockHitsTotal += blockHits;
      // The else branch should not be hit.
      const elseBranchHits = ranges[1].count;
      assert.strictEqual(elseBranchHits, 0);
    } else {
      // At process exit:
      assert.strictEqual(ranges.length, 3);
      const blockHits = ranges[0].count;
      // The block inside interval() should be hit at least once more.
      assert.notStrictEqual(blockHits, 0);
      blockHitsTotal += blockHits;
      // The else branch should be hit exactly once.
      const elseBranchHits = ranges[2].count;
      assert.strictEqual(elseBranchHits, 1);
      const ifBranchHits = ranges[1].count;
      assert.strictEqual(ifBranchHits, blockHits - elseBranchHits);
    }
  }

  // The block should be hit `intervals` times in total.
  assert.strictEqual(blockHitsTotal, intervals);
}
