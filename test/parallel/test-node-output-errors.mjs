import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import * as os from 'node:os';
import { describe, it } from 'node:test';

const skipForceColors =
  process.config.variables.icu_gyp_path !== 'tools/icu/icu-generic.gyp' ||
  process.config.variables.node_shared_openssl ||
  (common.isWindows && (Number(os.release().split('.')[0]) !== 10 || Number(os.release().split('.')[2]) < 14393)); // See https://github.com/nodejs/node/pull/33132

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('errors output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('//', '*')
      .replaceAll(process.version, '*')
      .replaceAll(/\/(\w)/g, '*$1')
      .replaceAll('*test*', '*')
      .replaceAll('*fixtures*errors*', '*')
      .replaceAll('file:**', 'file:*/');
  }

  function normalizeNoNumbers(str) {
    return normalize(str)
      .replaceAll(/:\d+/g, ':*');
  }
  
  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  const defaultTransform = snapshot.transform(common, normalize);
  const noNumberTransform = snapshot.transform(common, normalizeNoNumbers);
  const noStackTraceTransform = snapshot.transform(common, replaceStackTrace, normalizeNoNumbers);

  const tests = [
    { name: 'errors/async_error_eval_cjs.js' },
    { name: 'errors/async_error_eval_esm.js' },
    { name: 'errors/async_error_microtask_main.js' },
    { name: 'errors/async_error_nexttick_main.js' },
    { name: 'errors/async_error_sync_main.js' },
    { name: 'errors/async_error_sync_esm.mjs' },
    { name: 'errors/error_aggregateTwoErrors.js', transform: noNumberTransform },
    { name: 'errors/error_exit.js', transform: noNumberTransform },
    { name: 'errors/error_with_nul.js', transform: noNumberTransform },
    { name: 'errors/events_unhandled_error_common_trace.js', transform: noNumberTransform },
    { name: 'errors/events_unhandled_error_nexttick.js', transform: noNumberTransform },
    { name: 'errors/events_unhandled_error_sameline.js', transform: noNumberTransform },
    { name: 'errors/events_unhandled_error_subclass.js', transform: noNumberTransform },
    { name: 'errors/throw_custom_error.js', transform: noNumberTransform },
    { name: 'errors/throw_in_line_with_tabs.js', transform: noNumberTransform },
    { name: 'errors/throw_non_error.js', transform: noNumberTransform },
    { name: 'errors/promise_always_throw_unhandled.js', transform: noStackTraceTransform },
    { name: 'errors/if-error-has-good-stack.js', transform: noNumberTransform },
    { name: 'errors/test-no-extra-info-on-fatal-exception.js', transform: noStackTraceTransform },
    { name: 'errors/throw_error_with_getter_throw.js', transform: noStackTraceTransform },
    { name: 'errors/throw_null.js', transform: noStackTraceTransform },
    { name: 'errors/throw_undefined.js', transform: noStackTraceTransform },
    { name: 'errors/timeout_throw.js', transform: noStackTraceTransform },
    { name: 'errors/undefined_reference_in_new_context.js', transform: noStackTraceTransform },
    { name: 'errors/util_inspect_error.js', transform: noStackTraceTransform },
    { name: 'errors/util-inspect-error-cause.js', transform: noStackTraceTransform },
    { name: 'errors/v8_warning.js', transform: noStackTraceTransform },
    !skipForceColors ? { name: 'errors/force_colors.js', env: { FORCE_COLOR: 1 } } : null,
  ].filter(Boolean);

  for (const { name, transform, tty = false, env } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty: tty }, { env });
    });
  }
});
