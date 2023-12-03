import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

Error.stackTraceLimit = 0;

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

function normalize(str) {
  return str
  .replaceAll(process.version, '*')
  .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
  .replaceAll('/', '*')
  .replaceAll(/\d+/g, '*')
  .replaceAll('\x1B', '*')
  .replaceAll('process.processTicksAndRejections', '')
  .replaceAll('(node:internal*process*task_queues:*:*)', '')
  .replaceAll('node:internal*main*run_main_module:*:*', '');
  // .replace(/[^\x00-\x7F]/g, '90')
  // .replace(/\u001b\[\d+m/g, '39');
}
const common = snapshot
.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);

const defaultTransform = snapshot.transform(common, replaceStackTrace, normalize);

const tests = [
  { name: 'util/util-inspect-error-cause.js' },
  { name: 'util/util_inspect_error.js' },
];

describe('util output', { concurrency: true }, () => {
  for (const { name } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), defaultTransform);
    });
  }
});
