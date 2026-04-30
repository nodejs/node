// Test that the output of test-runner/output/name_and_skip_patterns.js matches
// test-runner/output/name_and_skip_patterns.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/name_and_skip_patterns.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
