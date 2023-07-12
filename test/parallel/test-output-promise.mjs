import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll(process.version, '*')
      .replace(/\(node:\d+\)/g, '(node:*)');
  }

  const common = snapshot.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  
  const defaultTransform = snapshot.transform(common, replaceStackTrace, normalize);
  
  const noTraceWarnings = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll(
        '(rejection id: 1)\n    at *\n    at *\n    at Promise.then (<anonymous>)\n    at Promise.catch (<anonymous>)\n    at *\n', 
        '(rejection id: 1)\n    at handledRejection (node:internal/process/promises:*)\n    at promiseRejectHandler (node:internal/process/promises:*)\n    at Promise.then *\n    at Promise.catch *\n    at Immediate.<anonymous> (*test*message*unhandled_promise_trace_warnings.js:*)\n')
      .replaceAll('This was rejected\n    at *\n', 'This was rejected\n    at * (*test*message*unhandled_promise_trace_warnings.js:*)\n')
  });
  
  const noWarnWithError = snapshot.transform(common, replaceStackTrace, (str) => {
    return normalize(str)
      .replaceAll('(node:*) ', '*')
      .replaceAll('Use `node --trace-warnings', 'Use `* --trace-warnings')
      .replaceAll('alas\n    at *\n', 'alas\n    at *promise_unhandled_warn_with_error.js:*:*\n');
  });

  const tests = [
    { name: 'promise/promise_unhandled_warn_with_error.js', transform: noWarnWithError },
    { name: 'promise/unhandled_promise_trace_warnings.js', transform: noTraceWarnings },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
