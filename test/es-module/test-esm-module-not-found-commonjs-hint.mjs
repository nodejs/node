import { spawnPromisified } from '../common/index.mjs';
import { fixturesDir } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: module not found hint', { concurrency: true }, () => {
  for (
    const { input, expected }
    of [
      {
        input: 'import "./print-error-message"',
        // Did you mean to import ../print-error-message.js?
        expected: / \.\.\/print-error-message\.js\?/,
      },
      {
        input: 'import obj from "some_module/obj"',
        expected: / some_module\/obj\.js\?/,
      },
    ]
  ) it('should cite a variant form', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      input,
    ], {
      cwd: fixturesDir,
    });

    match(stderr, expected);
    notStrictEqual(code, 0);
  });
});
