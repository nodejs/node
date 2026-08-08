// Test that the output of test-runner/output/dot_reporter_coverage.mjs matches
// test-runner/output/dot_reporter_coverage.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/dot_reporter_coverage.mjs'),
  defaultTransform,
  { flags: ['--test-reporter=dot', '--test-coverage-exclude=!test/**'] },
);
