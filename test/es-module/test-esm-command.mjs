import { spawnPromisified } from '../common/index.mjs';
import { strictEqual } from 'node:assert';
import { fileURLToPath } from 'node:url';

{
  const { code, stderr, stdout } = await spawnPromisified(process.execPath, [
    '--input-type',
    'module',
    '-e',
    'console.log(import.meta.command)',
  ]);

  if (code !== 0) {
    console.error(stderr.toString());
  }

  strictEqual(stdout.toString().trim(), 'true');
}

{
  const { code, stderr, stdout } = await spawnPromisified(process.execPath, [
    fileURLToPath(new URL('../fixtures/es-modules/command-main.mjs', import.meta.url)),
  ]);

  if (code !== 0) {
    console.error(stderr.toString());
  }

  strictEqual(stdout.toString().trim(), 'ok');
}
