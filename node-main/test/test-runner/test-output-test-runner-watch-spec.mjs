// Test that the output of test-runner/output/test-runner-watch-spec.mjs matches
// test-runner/output/test-runner-watch-spec.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/test-runner-watch-spec.mjs'),
  specTransform,
);
