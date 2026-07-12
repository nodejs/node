// Test that the output of test-runner/output/skip-each-hooks.js matches
// test-runner/output/skip-each-hooks.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/skip-each-hooks.js'),
  specTransform,
);
