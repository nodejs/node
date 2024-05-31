import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import fixtures from '../common/fixtures.js';

import { it } from 'node:test';

import assert from 'assert';

const files = [
  'close.mjs',
  'before-exit.mjs',
  'gc-not-close.mjs',
  'unregister.mjs',
];

for (const file of files) {
  it(`should exit file ${file} with code=0`, async () => {
    spawnSyncAndAssert(process.execPath, ['--expose-gc', `${file}`], {
      cwd: fixtures.path('process'),
    }, {
      code: 0,
    });
  });
}

it('should throw when register undefined value', () => {
  try {
    process.registerFreeOnExit(undefined);

    assert.fail('Expected an error to be thrown for registerFreeOnExit');
  } catch (e) {
    assert.ok(e.message.includes('must be not undefined'), `Expected error message to include 'Invalid' but got: ${e.message}`);
  }

  try {
    process.registerFreeOnBeforeExit(undefined);

    assert.fail('Expected an error to be thrown for registerFreeOnBeforeExit');
  } catch (e) {
    assert.ok(e.message.includes('must be not undefined'), `Expected error message to include 'Invalid' but got: ${e.message}`);
  }
});
