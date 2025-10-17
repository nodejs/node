import { isWindows } from '../common/index.mjs';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { fileURLToPath } from 'node:url';
import { strictEqual } from 'node:assert';

const python = process.env.PYTHON || (isWindows ? 'python' : 'python3');

const testRunner = fileURLToPath(
  new URL('../../tools/test.py', import.meta.url)
);

const proc = spawn(python, [
  testRunner,
  `--mode=${process.features.debug ? 'debug' : 'release'}`,
  `--shell=${process.execPath}`,
  '--node-args=--no-async-context-frame',
  '*/test-async-local-storage-*',
], {
  stdio: ['inherit', 'inherit', 'inherit'],
});

const [code] = await once(proc, 'exit');
strictEqual(code, 0);
