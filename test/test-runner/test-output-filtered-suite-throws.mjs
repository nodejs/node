// Test that the output of test-runner/output/filtered-suite-throws.js matches
// test-runner/output/filtered-suite-throws.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/filtered-suite-throws.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
