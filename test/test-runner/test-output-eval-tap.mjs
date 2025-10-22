// Test that the output of test-runner/output/eval_tap.js matches test-runner/output/eval_tap.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/eval_tap.js'),
  defaultTransform,
);
