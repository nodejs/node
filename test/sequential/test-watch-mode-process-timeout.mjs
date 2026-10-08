// Tests that in watch mode --process-timeout applies to each run of the
// application, but not to the process that watches for changes.

import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { writeFileSync } from 'node:fs';
import { setTimeout } from 'node:timers/promises';
import { createInterface } from 'node:readline';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

tmpdir.refresh();
const file = tmpdir.resolve('hang.js');
writeFileSync(file, 'setInterval(() => {}, 60_000);');

const timeout = common.platformTimeout(500);
const child = spawn(process.execPath, [
  '--watch',
  '--no-warnings',
  `--process-timeout=${timeout}ms`,
  file,
], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => { stderr += data; });

for await (const line of createInterface({ input: child.stdout })) {
  if (line.startsWith('Failed running')) break;
}

// The watcher is still running well after its own deadline would have passed.
await setTimeout(timeout);
assert.strictEqual(child.exitCode, null);

child.kill();
await once(child, 'close');
// What the run was doing depends on how quickly it started, so only check
// that it timed out.
assert.match(stderr, new RegExp(
  `^\\(node:\\d+\\) Process timed out after ${timeout}ms \\(--process-timeout\\)\\. ` +
  'Exiting with code 124\\.$', 'm'));
