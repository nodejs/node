// Test run({ watch: true, cwd, isolation: 'none' }) does not reuse the
// parent process argv when spawning the watch child.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';
import { writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { run } from 'node:test';
import tmpdir from '../common/tmpdir.js';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();
tmpdir.refresh();

await writeFile(join(tmpdir.path, 'test.js'), `
const test = require('node:test');

test('test ran from cwd', () => {});
`);

// Add some delay to ensure the OS sends the FS events before watch mode is started.
await setTimeout(common.platformTimeout(100));

const passed = [];
const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
  watch: true,
  signal: controller.signal,
  isolation: 'none',
}).on('data', ({ type }) => {
  if (type !== 'test:watch:drained') return;

  stream.removeAllListeners('test:fail');
  stream.removeAllListeners('test:pass');
  controller.abort();
});

stream.on('test:watch:restarted', common.mustNotCall('test:watch:restarted'));
stream.on('test:fail', common.mustNotCall('test:fail'));
stream.on('test:pass', common.mustCall((data) => passed.push(data.name)));

// eslint-disable-next-line no-empty-pattern
for await (const {} of stream);

// Validate the expected test ran by name:
assert.deepStrictEqual(passed, ['test ran from cwd']);
