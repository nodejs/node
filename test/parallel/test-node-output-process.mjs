import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('process output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'process/max_tick_depth.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), snapshot.defaultTransform);
    });
  }
});
