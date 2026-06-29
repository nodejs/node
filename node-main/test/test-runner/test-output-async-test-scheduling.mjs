// Test that the output of test-runner/output/async-test-scheduling.mjs matches
// test-runner/output/async-test-scheduling.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/async-test-scheduling.mjs'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
