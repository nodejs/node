import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import * as os from 'node:os';
import { describe, it } from 'node:test';

const skipForceColors =
  (common.isWindows && (Number(os.release().split('.')[0]) !== 10 || Number(os.release().split('.')[2]) < 14393)); // See https://github.com/nodejs/node/pull/33132

describe('errors output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  const tests = [
    { name: 'errors/async_error_eval_cjs.js' },
    { name: 'errors/async_error_eval_esm.js' },
    { name: 'errors/async_error_microtask_main.js' },
    { name: 'errors/async_error_nexttick_main.js' },
    { name: 'errors/async_error_sync_main.js' },
    { name: 'errors/core_line_numbers.js' },
    { name: 'errors/async_error_sync_esm.mjs' },
    { name: 'errors/test-no-extra-info-on-fatal-exception.js' },
    { name: 'errors/error_aggregateTwoErrors.js' },
    { name: 'errors/error_exit.js' },
    { name: 'errors/error_with_nul.js' },
    { name: 'errors/events_unhandled_error_common_trace.js' },
    { name: 'errors/events_unhandled_error_nexttick.js' },
    { name: 'errors/events_unhandled_error_sameline.js' },
    { name: 'errors/events_unhandled_error_subclass.js' },
    { name: 'errors/if-error-has-good-stack.js' },
    { name: 'errors/throw_custom_error.js' },
    { name: 'errors/throw_error_with_getter_throw.js' },
    { name: 'errors/throw_in_eval_anonymous.js' },
    { name: 'errors/throw_in_eval_named.js' },
    { name: 'errors/throw_in_line_with_tabs.js' },
    { name: 'errors/throw_non_error.js' },
    { name: 'errors/throw_null.js' },
    { name: 'errors/throw_undefined.js' },
    { name: 'errors/timeout_throw.js' },
    { name: 'errors/undefined_reference_in_new_context.js' },
    { name: 'errors/promise_always_throw_unhandled.js' },
    { name: 'errors/promise_unhandled_warn_with_error.js' },
    { name: 'errors/unhandled_promise_trace_warnings.js' },
    { skip: skipForceColors, name: 'errors/force_colors.js', env: { FORCE_COLOR: 1 } },
  ];
  for (const { name, env, skip = false } of tests) {
    it(name, { skip }, async () => {
      await snapshot.spawnAndAssert(
        fixtures.path(name),
        snapshot.defaultTransform,
        { env: { ...env, ...process.env } }
      );
    });
  }
});
