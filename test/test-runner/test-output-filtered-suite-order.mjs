// Test that the output of test-runner/output/filtered-suite-order.mjs matches
// test-runner/output/filtered-suite-order.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/filtered-suite-order.mjs'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
