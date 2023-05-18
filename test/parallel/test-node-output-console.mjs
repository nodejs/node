import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

const skipForceColors =
  process.config.variables.icu_gyp_path !== 'tools/icu/icu-generic.gyp' ||
  process.config.variables.node_shared_openssl;

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('console output', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '').replaceAll('/', '*').replaceAll(process.version, '*').replaceAll(/\d+/g, '*').replaceAll('[*m', '');
  }
  const tests = [
    { name: 'console/2100bytes.js' },
    { name: 'console/console_low_stack_space.js' },
    { name: 'console/console.js' },
    { name: 'console/core_line_numbers.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, normalize),
      tty: true
    },
    { name: 'console/eval_messages.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, normalize),
      tty: true
    },
    { name: 'console/hello_world.js' },
    {
      name: 'console/stack_overflow.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, normalize)
    },
    { name: 'console/stdin_messages.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, normalize),
      tty: true
    },
    { name: 'console/stdin_messages.js',
      transform: snapshot
        .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceNodeVersion, stackTrace),
      tty: true
    },
    !skipForceColors ? { name: 'console/force_colors.js', env: { FORCE_COLOR: 1 } } : null,
  ].filter(Boolean);
  const defaultTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceStackTrace);
  for (const { name, transform, tty = false, env } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty: tty }, { env });
    });
  }
});
