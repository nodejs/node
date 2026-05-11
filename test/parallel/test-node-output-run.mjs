import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('node --run output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'run-script/node_run_non_existent.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), snapshot.defaultTransform);
    });
  }
});
