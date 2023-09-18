import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';

import {
  deepStrictEqual,
  match,
  strictEqual,
} from 'node:assert';
import fs from 'node:fs/promises';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: non-js extensions', { concurrency: true }, () => {
  describe('when no handler is installed', { concurrency: true }, () => {
    it('should throw on unknown file extension', async () => {
      const { code, stderr, signal } = await spawnPromisified(execPath, [
        '--input-type=module',
        '--eval',
        `import ${JSON.stringify(fixtures.fileURL('es-modules', 'file.unknown'))}`,
      ]);

      match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
      strictEqual(code, 1);
      strictEqual(signal, null);
    });

    it('should identify format and handle .ts', async () => {
      const { code, stderr, signal } = await spawnPromisified(execPath, [
        '--input-type=module',
        '--eval',
        `import ${JSON.stringify(fixtures.fileURL('empty.ts'))}`,
      ]);

      match(stderr, /ERR_UNSUPPORTED_MODULE_FORMAT/);
      match(stderr, /typescript/);
      match(stderr, /install/);
      match(stderr, /nodejs\.org/);
      strictEqual(code, 1);
      strictEqual(signal, null);
    });
  });

  it('when handler is installed', { concurrency: true }, async (t) => {
    const cwd = tmpdir.resolve('ts-test' + Math.random());

    try {
      tmpdir.refresh();
      const nodeModules = path.join(cwd, 'node_modules');
      await fs.mkdir(nodeModules, { recursive: true });
      await fs.cp(
        fixtures.fileURL('external-modules', 'web-loader'),
        path.join(nodeModules, 'web-loader'),
        { recursive: true },
      );
      await fs.writeFile(path.join(cwd, 'main.ts'), 'const foo: number = 1; console.log(foo);\n');
      await fs.writeFile(path.join(cwd, '.env'), 'NODE_OPTIONS=--import=web-loader\n');

      await t.test('should identify format and handle .ts', async () => {
        const output = await spawnPromisified(execPath, [
          '--env-file=.env',
          'main.ts',
        ], { cwd });

        deepStrictEqual(output, {
          code: 0,
          signal: null,
          stderr: '',
          stdout: '1\n',
        });
      });
    } finally {
      await fs.rm(cwd, { recursive: true, force: true });
    }
  });
});
