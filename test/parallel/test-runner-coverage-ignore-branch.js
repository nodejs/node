'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const skipIfNoInspector = {
  skip: !process.features.inspector ? 'inspector disabled' : false
};

test('coverage ignore next excludes BRDA for ignored branches', skipIfNoInspector, async (t) => {
  const fixture = fixtures.path('test-runner', 'coverage-ignore-branch.js');
  const args = [
    '--experimental-test-coverage',
    '--test-reporter', 'lcov',
    fixture,
  ];
  const result = spawnSync(process.execPath, args);
  const lcovOutput = result.stdout.toString();

  // Parse the LCOV output
  const lines = lcovOutput.split('\n');
  const brdaLines = lines.filter(l => l.startsWith('BRDA:'));

  // All branches should be covered (no uncovered branches)
  // The branch leading to the ignored code should be excluded
  const uncoveredBranches = brdaLines.filter(l => l.endsWith(',0'));

  assert.strictEqual(uncoveredBranches.length, 0,
    `Expected no uncovered branches, but found: ${uncoveredBranches.join(', ')}`);

  // Verify branch coverage is 100%
  const brfLine = lines.find(l => l.startsWith('BRF:'));
  const brhLine = lines.find(l => l.startsWith('BRH:'));
  assert(brfLine, 'Should have BRF line');
  assert(brhLine, 'Should have BRH line');

  const brf = parseInt(brfLine.split(':')[1], 10);
  const brh = parseInt(brhLine.split(':')[1], 10);

  assert.strictEqual(brf, brh,
    `Expected branch coverage to be 100% (${brh}/${brf}), but found ${brh}/${brf}`);
});
