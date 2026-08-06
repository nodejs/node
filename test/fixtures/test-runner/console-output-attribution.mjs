import { Console } from 'node:console';
import { before, beforeEach, describe, test } from 'node:test';

console.log('toplevel-console-7b31');
before(() => console.log('globalhook-console-52ac'));

describe('concurrent tests', { concurrency: true }, () => {
  console.log('suitedef-console-91de');
  before(() => console.log('suitehook-console-4f0a'));
  beforeEach((t) => console.log(`beforeeach-console-${t.name}-6c2b`));

  test('first', async () => {
    console.log('attributed-out-first-a17e');
    const customConsole = new Console({
      stdout: process.stdout,
      stderr: process.stderr,
    });
    customConsole.log('customconsole-out-3e75');
    process.stdout.write('directstream-out-first-d40c\n');
    await new Promise((resolve) => setImmediate(resolve));
    console.error('attributed-err-first-b85f');
    process.stderr.write('directstream-err-first-e93a\n');
  });

  test('second', () => {
    console.log('attributed-out-second-c62d');
    setImmediate(() => console.log('late-console-f08b'));
  });
});

// A test declared through eval() has no source location, so its events carry
// no line or column. Its output still has to report a file.
eval(`test('no source location', () => {
  console.log('attributed-out-noloc-5ad9');
});`);
