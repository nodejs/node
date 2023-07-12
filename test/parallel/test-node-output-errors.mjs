import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import * as os from 'node:os';
import { describe, it } from 'node:test';

const skipForceColors =
  process.config.variables.icu_gyp_path !== 'tools/icu/icu-generic.gyp' ||
  process.config.variables.node_shared_openssl ||
  (common.isWindows && (Number(os.release().split('.')[0]) !== 10 || Number(os.release().split('.')[2]) < 14393)); // See https://github.com/nodejs/node/pull/33132

function removeSpecialCharacter(str) {
  return str.replaceAll('', '');
}

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(removeSpecialCharacter(str), '$1at *$7\n');
}

function replaceStackTraceCustom(str) {
  return snapshot.replaceStackTrace(removeSpecialCharacter(str), '$1$3 $4:$5:$6*\n');
}

function replaceStackTrace2(str) {
  return removeSpecialCharacter(str).replace(/(\s+)((.+?)\s+\()?(?:\(?(.+?):(\d+)(?::(\d+))?)\)?(\s+\{)?(\[\d+m)?(\n|$)?/g, '$1at *$7\n');
}

describe('errors output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('/test/fixtures/errors/', '*')
      .replaceAll(process.version, '*')
      .replaceAll('/', '*')
      .replace(/\(node:\d+\)/g, '(node:*)')
      .replaceAll(' ** ', ' * ')
      .replaceAll('*test*fixtures*', '*fixtures*')
      .replaceAll('file:***', 'file:*/');
  }

  function normalizeNoNumbers(str) {
    return normalize(str)
      .replaceAll(/:\d+/g, ':*');
  }
  
  const common = snapshot.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  
  const defaultTransform = snapshot.transform(common, normalize);
  
  const noNumberTransform = snapshot.transform(common, normalizeNoNumbers);
  const noStackTraceTransform = snapshot.transform(common, replaceStackTrace, normalizeNoNumbers);

  const noFatalExceptionTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
    .replaceAll(/\*test-no-extra-info-on-fatal-exception.js:(\d+)(:\d+)?\*?\n/g, '*:$1$2\n')
    .replaceAll(/(\*:\d+:\d+)\n/g, '($1)');
  });

  const noGetterThrowTransform = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll('Use `node --trace-uncaught', 'Use `* --trace-uncaught')
      .replaceAll('Node.js *\n', 'Node.js *')
      .replaceAll('[object Object]\n', 'null\n')
      .replaceAll('at *\nthrow {  * eslint-disable-line no-throw-literal\n', '*test*message*throw_null.js:*\nthrow null;\n');
  });

  const noThrowNullTransform = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll('Use `node --trace-uncaught', 'Use `* --trace-uncaught')
      .replaceAll('at *\n', '*test*message*throw_null.js:*\n');
  });

  const noThrowUndefinedTransform = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll('Use `node --trace-uncaught', 'Use `* --trace-uncaught')
      .replaceAll('at *\n', '*test*message*throw_undefined.js:*\n');
  });

  const noTimeoutThrowTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
      .replaceAll('^\n\n', '^\n')
      .replaceAll(/\*timeout_throw.js:\d+(:\d+)?/g, '*test*message*timeout_throw.js:*')
      .replaceAll('_onTimeout *test*message*timeout_throw.js:**\n', '_onTimeout (*test*message*timeout_throw.js:*:*)\n')
      .replaceAll(/node:internal\*timers:\d+:\d+\*\n/g, '(node:internal/timers:*:*)\n')
  });

  const noReferenceNewContextTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
      .replaceAll(/node:vm:\d+:\d+\*/g, '(node:vm:*)')
      .replaceAll(/\s{5}at\sevalmachine.<anonymous>:\d+:\d+\*\n/g, '    at evalmachine.<anonymous>:*:*\n')
      .replaceAll(/\sevalmachine.<anonymous>:(\d+):\*\n/g, 'evalmachine.<anonymous>:$1\n')
      .replaceAll(/\*undefined_reference_in_new_context.js:\d+:\d+\*/g, '(*test*message*undefined_reference_in_new_context.js:*)');
  });

  const noErrorCauseTransform = snapshot.transform(common, replaceStackTrace2, (str) => {
    return normalize(str)
      // .replaceAll('  at *\n', '')
      .replaceAll('Error: Unique/n\' +\n      at *\n\'\n', 'Error: Unique\\n\' +\n      \'    at Module._compile (node:internal/modules/cjs/loader:827:30)\'\n')
      .replaceAll('[90m    at *\n {\n', '[90m    at [39m {\n')
      .replaceAll(')[39m\n  at *\n', '  [90m    at [39m\n')
      .replaceAll(/\)\[39m\n(:?\s*at\s\*\n)/g, '')
      .replaceAll(/\[(\d+m)/g, '*[$1');
  });

  const noV8WarningTransform = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll('/', '*')
      .replaceAll(/:\d+/g, ':*')
      .replaceAll('`node --trace-warnings ...`', '`* --trace-warnings ...`');
  });

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
    { name: 'errors/test-no-extra-info-on-fatal-exception.js', transform: noFatalExceptionTransform },
    { name: 'errors/throw_error_with_getter_throw.js', transform: noGetterThrowTransform },
    { name: 'errors/throw_null.js', transform: noThrowNullTransform },
    { name: 'errors/throw_undefined.js', transform: noThrowUndefinedTransform },
    { name: 'errors/timeout_throw.js', transform: noTimeoutThrowTransform },
    { name: 'errors/undefined_reference_in_new_context.js', transform: noReferenceNewContextTransform },
    { name: 'errors/util_inspect_error.js', transform: noStackTraceTransform },
    { name: 'errors/util-inspect-error-cause.js', transform: noErrorCauseTransform },
    { name: 'errors/v8_warning.js', transform: noV8WarningTransform },
    !skipForceColors ? { name: 'errors/force_colors.js', env: { FORCE_COLOR: 1 } } : null,
  ].filter(Boolean);

  for (const { name, transform, tty = false, env } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty: tty }, { env });
    });
  }
});
