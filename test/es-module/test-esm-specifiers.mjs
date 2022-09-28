import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { match, strictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: specifier-resolution=node', { concurrency: true }, () => {
  it(async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-specifier-resolution=node',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        // CommonJS index.js
        `import commonjs from ${JSON.stringify(fixtures.fileURL('es-module-specifiers/package-type-commonjs'))};`,
        // ESM index.js
        `import module from ${JSON.stringify(fixtures.fileURL('es-module-specifiers/package-type-module'))};`,
        // Directory entry with main.js
        `import main from ${JSON.stringify(fixtures.fileURL('es-module-specifiers/dir-with-main'))};`,
        // Notice the trailing slash
        `import success, { explicit, implicit, implicitModule } from ${JSON.stringify(fixtures.fileURL('es-module-specifiers/'))};`,
        'strictEqual(commonjs, "commonjs");',
        'strictEqual(module, "module");',
        'strictEqual(main, "main");',
        'strictEqual(success, "success");',
        'strictEqual(explicit, "esm");',
        'strictEqual(implicit, "cjs");',
        'strictEqual(implicitModule, "cjs");',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should throw when the file doesn\'t exist', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-module-specifiers/do-not-exist.js'),
    ]);

    match(stderr, /Cannot find module/);
    strictEqual(stdout, '');
    strictEqual(code, 1);
  });

  it('should throw when the omitted file extension is .mjs (legacy loader doesn\'t support it)', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-specifier-resolution=node',
      '--input-type=module',
      '--eval',
      `import whatever from ${JSON.stringify(fixtures.fileURL('es-module-specifiers/implicit-main-type-commonjs'))};`,
    ]);

    match(stderr, /ERR_MODULE_NOT_FOUND/);
    strictEqual(stdout, '');
    strictEqual(code, 1);
  });

  for (
    const item of [
      'package-type-commonjs',
      'package-type-module',
      '/',
      '/index',
    ]
  ) it('should ', async () => {
    const { code } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-specifier-resolution=node',
      '--es-module-specifier-resolution=node',
      fixtures.path('es-module-specifiers', item),
    ]);

    strictEqual(code, 0);
  });
});
