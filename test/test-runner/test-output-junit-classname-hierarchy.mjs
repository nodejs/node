// Test that the output of test-runner/output/junit_classname_hierarchy.js matches
// test-runner/output/junit_classname_hierarchy.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, junitTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/junit_classname_hierarchy.js'),
  junitTransform,
  { flags: ['--test-reporter=junit'] },
);
