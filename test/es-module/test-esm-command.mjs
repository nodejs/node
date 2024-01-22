import { spawnPromisified } from '../common/index.mjs';
import { spawnSync } from 'node:child_process';
import { strictEqual } from 'node:assert';
import { fileURLToPath } from 'node:url';

{
  const { code, stderr, stdout } = await spawnPromisified(process.execPath, [
    '--input-type',
    'module',
    '-e',
    'console.log(import.meta.command)',
  ]);

  if (child.status !== 0) {
    console.error(child.stderr.toString());
  }

  strictEqual(child.stdout.toString().trim(), 'true');
}

{
  const { code, stderr, stdout } = await spawnPromisified(process.execPath, [
    fileURLToPath(new URL('../fixtures/es-modules/command-main.mjs', import.meta.url)),
  ]);

  if (child.status !== 0) {
    console.error(child.stderr?.toString() ?? child.error);
  }

  strictEqual(child.stdout.toString().trim(), 'ok');
}
