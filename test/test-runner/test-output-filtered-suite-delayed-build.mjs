// Test that the output of test-runner/output/filtered-suite-delayed-build.js matches
// test-runner/output/filtered-suite-delayed-build.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/filtered-suite-delayed-build.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
