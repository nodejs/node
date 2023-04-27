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

describe('console output', { concurrency: true }, () => {
  function stackTrace(str) {
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '').replaceAll('/', '*').replaceAll(/\d+/g, '*');
  }
  const tests = [
    { name: 'console/2100bytes.js' },
    { name: 'console/console_low_stack_space.js' },
    { name: 'console/console.js' },
    { name: 'console/hello_world.js' },
    {
      name: 'console/stack_overflow.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceNodeVersion, stackTrace)
    },
  ];
  const defaultTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceStackTrace);
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
