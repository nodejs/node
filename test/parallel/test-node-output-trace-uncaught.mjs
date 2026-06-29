import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

describe('--trace-uncaught', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'uncaught/exception_async_tick.js' },
    { name: 'uncaught/exception_null.js' },
    { name: 'uncaught/exception_stack_malformed.js' },
    { name: 'uncaught/exception_symbol.js' },
    { name: 'uncaught/exception_undefined.js' },
    { name: 'uncaught/rejection_async_fn_throw_err.js' },
    { name: 'uncaught/rejection_async_fn_throw.js' },
    { name: 'uncaught/rejection_promise_reject.js' },
  ];
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), snapshot.defaultTransform, {
        flags: ['--trace-uncaught'],
      });
    });
  }
});
