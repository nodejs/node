import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceNodeVersion(str) {
  return str.replaceAll(process.version, '*');
}

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('console', { concurrency: true }, () => {
  function stackTrace(str) {
    return str.replaceAll(process.cwd(), '').replaceAll('/', '*').replaceAll(/\d+/g, '*');
  }
  const tests = [
    { name: 'message/2100bytes.js' },
    { name: 'message/console_low_stack_space.js' },
    { name: 'message/console.js' },
    { name: 'message/hello_world.js' },
    {
      name: 'message/stack_overflow.js',
      transform: snapshot.transform(snapshot.replaceWindowsLineEndings, replaceNodeVersion, stackTrace)
    },
  ];
  const defaultTransform = snapshot.transform(snapshot.replaceWindowsLineEndings, replaceStackTrace);
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});

describe('errors', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(process.cwd(), '').replaceAll('//', '*').replaceAll(/\/(\w)/g, '*$1').replaceAll('*test*', '*');
  }

  function normalizeNoNumbers(str) {
    return normalize(str).replaceAll(/\d+:\d+/g, '*:*').replaceAll(/:\d+/g, ':*').replaceAll('*fixtures*message*', '*');
  }
  const defaultTransform = snapshot.transform(snapshot.replaceWindowsLineEndings, replaceNodeVersion, normalize);
  const errTransform = snapshot.transform(snapshot.replaceWindowsLineEndings, replaceNodeVersion, normalizeNoNumbers);
  const promiseTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, replaceStackTrace, normalizeNoNumbers, replaceNodeVersion);

  const tests = [
    { name: 'message/async_error_eval_cjs.js' },
    { name: 'message/async_error_eval_esm.js' },
    { name: 'message/async_error_microtask_main.js' },
    { name: 'message/async_error_nexttick_main.js' },
    { name: 'message/async_error_sync_main.js' },
    { name: 'message/async_error_sync_esm.mjs' },
    { name: 'message/error_aggregateTwoErrors.js', transform: errTransform },
    { name: 'message/error_exit.js', transform: errTransform },
    { name: 'message/error_with_nul.js', transform: errTransform },
    { name: 'message/events_unhandled_error_common_trace.js', transform: errTransform },
    { name: 'message/events_unhandled_error_nexttick.js', transform: errTransform },
    { name: 'message/events_unhandled_error_sameline.js', transform: errTransform },
    { name: 'message/events_unhandled_error_subclass.js', transform: errTransform },
    { name: 'message/throw_custom_error.js', transform: errTransform },
    { name: 'message/throw_in_line_with_tabs.js', transform: errTransform },
    { name: 'message/throw_non_error.js', transform: errTransform },
    { name: 'message/promise_always_throw_unhandled.js', transform: promiseTransform },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});

describe('sourcemaps', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(process.cwd(), '')
    .replaceAll('//', '*')
    .replaceAll('/Users/bencoe/oss/coffee-script-test', '')
    .replaceAll(/\/(\w)/g, '*$1')
    .replaceAll('*test*', '*')
    .replaceAll('*fixtures*source-map*', '*');
  }
  const defaultTransform = snapshot.transform(snapshot.replaceWindowsLineEndings, replaceNodeVersion, normalize);

  const tests = [
    { name: 'message/source_map_disabled_by_api.js' },
    { name: 'message/source_map_enabled_by_api.js' },
    { name: 'message/source_map_eval.js' },
    { name: 'message/source_map_no_source_file.js' },
    { name: 'message/source_map_throw_first_tick.js' },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});

describe('vm', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(process.cwd(), '').replaceAll('//', '*').replaceAll(/\/(\w)/g, '*$1').replaceAll('*test*', '*');
  }

  function normalizeNoNumbers(str) {
    return normalize(str).replaceAll(/node:vm:\d+:\d+/g, 'node:vm:*');
  }
  const defaultTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, replaceNodeVersion, normalizeNoNumbers);

  const tests = [
    { name: 'message/vm_caught_custom_runtime_error.js' },
    { name: 'message/vm_display_runtime_error.js' },
    { name: 'message/vm_display_syntax_error.js' },
    { name: 'message/vm_dont_display_runtime_error.js' },
    { name: 'message/vm_dont_display_syntax_error.js' },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
