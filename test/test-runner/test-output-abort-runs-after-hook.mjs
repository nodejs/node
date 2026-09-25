// Test that the output of test-runner/output/abort-runs-after-hook.js matches
// test-runner/output/abort-runs-after-hook.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/abort-runs-after-hook.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
