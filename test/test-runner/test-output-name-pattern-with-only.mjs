// Test that the output of test-runner/output/name_pattern_with_only.js matches
// test-runner/output/name_pattern_with_only.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/name_pattern_with_only.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
