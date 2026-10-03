import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('event_loop output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'event_loop/max_tick_depth.js' },
    { name: 'event_loop/nexttick_throw.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(
        fixtures.path(name),
        snapshot.defaultTransform,
      );
    });
  }
});
