// Test that the output of test-runner/output/skip_pattern.js matches test-runner/output/skip_pattern.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/skip_pattern.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
