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

describe('errors output', { concurrency: true }, () => {
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
    { name: 'errors/async_error_eval_cjs.js' },
    { name: 'errors/async_error_eval_esm.js' },
    { name: 'errors/async_error_microtask_main.js' },
    { name: 'errors/async_error_nexttick_main.js' },
    { name: 'errors/async_error_sync_main.js' },
    { name: 'errors/async_error_sync_esm.mjs' },
    { name: 'errors/error_aggregateTwoErrors.js', transform: errTransform },
    { name: 'errors/error_exit.js', transform: errTransform },
    { name: 'errors/error_with_nul.js', transform: errTransform },
    { name: 'errors/events_unhandled_error_common_trace.js', transform: errTransform },
    { name: 'errors/events_unhandled_error_nexttick.js', transform: errTransform },
    { name: 'errors/events_unhandled_error_sameline.js', transform: errTransform },
    { name: 'errors/events_unhandled_error_subclass.js', transform: errTransform },
    { name: 'errors/throw_custom_error.js', transform: errTransform },
    { name: 'errors/throw_in_line_with_tabs.js', transform: errTransform },
    { name: 'errors/throw_non_error.js', transform: errTransform },
    { name: 'errors/promise_always_throw_unhandled.js', transform: promiseTransform },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
