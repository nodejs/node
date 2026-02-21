// Test that the output of test-runner/output/no_tests.js matches test-runner/output/no_tests.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/no_tests.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
