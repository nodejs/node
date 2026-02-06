// Test that the output of test-runner/output/bail_concurrency_1.js matches test-runner/output/bail_concurrency_1.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/bail_concurrency_1.js'),
  specTransform,
);
