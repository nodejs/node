import { spawnPromisified } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import fs from 'node:fs/promises';
import { execPath } from 'node:process';

tmpdir.refresh();
const target = tmpdir.fileURL(`${Math.random()}.mjs`);

await assert.rejects(import(target), { code: 'ERR_MODULE_NOT_FOUND' });

await fs.writeFile(target, 'export default "actual target"\n');

const moduleRecord = await import(target);

await fs.rm(target);

assert.strictEqual(await import(target), moduleRecord);

// Add the file back, it should be deleted by the subprocess.
await fs.writeFile(target, 'export default "actual target"\n');

assert.deepStrictEqual(
  await spawnPromisified(execPath, [
    '--input-type=module',
    '--eval',
    [`import * as d from${JSON.stringify(target)};`,
     'import{rm}from"node:fs/promises";',
     `await rm(new URL(${JSON.stringify(target)}));`,
     'import{strictEqual}from"node:assert";',
     `strictEqual(JSON.stringify(await import(${JSON.stringify(target)})),JSON.stringify(d));`].join(''),
  ]),
  {
    code: 0,
    signal: null,
    stderr: '',
    stdout: '',
  });
