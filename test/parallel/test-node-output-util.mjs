import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('util output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'util/util_inspect_error.js', transform: snapshot.defaultTransform },
    { name: 'util/util-inspect-error-cause.js', transform: snapshot.defaultTransform },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform);
    });
  }
});
