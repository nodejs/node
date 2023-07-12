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

function replaceStackTraceCustom(str) {
  return snapshot.replaceStackTrace(str, '$1$3 $4:$5:$6\n');
}

describe('console output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      // .replaceAll('/', '*')
      .replaceAll(process.version, '*')
      .replaceAll(/\d+/g, '*');
  }

  const common = snapshot.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  
  const defaultTransform = snapshot.transform(common, replaceStackTrace);
  
  const normalizedTransform = snapshot.transform(common, replaceStackTrace, normalize);

  const noEvalMessagesTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
      .replaceAll('at *\n', '')
      .replaceAll(' [eval]:*:', '[eval]:1')
      .replaceAll(/\s{5}at\s/g, '    at ')
      .replaceAll('^\n\n', '^\n')
      .replaceAll('Strict mode code may not include a with statement', 'Strict mode code may not include a with statement\n')
      .replaceAll(/\snode:vm:\*:\*(\n)?/g, ' (node:vm:*:*)\n')
      .replaceAll('*    at ', '*\n    at ')
      .replaceAll(/Script\s(node:internal\/process\/execution:\*:\*)/g, 'Script ($1)')
      .replaceAll('Node.js *\n*\n*\n', 'Node.js *\n42\n42\n')
      .replaceAll('Error: hello', '\nError: hello');
  });

  const noMessagesTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
      .replaceAll(/\s\[stdin\]:\*:/g, '[stdin]:1')
      .replaceAll(/\s{5}at\s/g, '    at ')
      .replaceAll('    at [stdin]:1*\n', '    at [stdin]:1:7\n')
      .replaceAll(' node:vm:*:*', ' (node:vm:*)');
  });

  const tests = [
    { name: 'console/2100bytes.js' },
    { name: 'console/console_low_stack_space.js' },
    { name: 'console/console.js' },
    { name: 'console/core_line_numbers.js', transform: normalizedTransform },
    { name: 'console/eval_messages.js', transform: noEvalMessagesTransform },
    { name: 'console/hello_world.js' },
    { name: 'console/stack_overflow.js', transform: normalizedTransform },
    { name: 'console/stdin_messages.js', transform: noMessagesTransform },
    !skipForceColors ? { name: 'console/force_colors.js', env: { FORCE_COLOR: 1 } } : null,
  ].filter(Boolean);

  for (const { name, transform, tty = false, env } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty: tty }, { env });
    });
  }
});
