// Test that the output of test-runner/output/suite-skip-hooks.js matches
// test-runner/output/suite-skip-hooks.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/suite-skip-hooks.js'),
  specTransform,
);
