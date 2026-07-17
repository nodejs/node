// Test that the output of test-runner/output/spec_reporter_successful.js matches
// test-runner/output/spec_reporter_successful.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/spec_reporter_successful.js'),
  specTransform,
);
