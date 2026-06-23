// Test run({ watch: true, cwd, isolation: 'none' }) does not reuse the
// parent process argv when spawning the watch child.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { run } from 'node:test';
import tmpdir from '../common/tmpdir.js';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();
tmpdir.refresh();

const watchedDir = join(tmpdir.path, 'watched');
const marker = join(tmpdir.path, 'marker');
mkdirSync(watchedDir);
writeFileSync(join(watchedDir, 'test.js'), `
const test = require('node:test');
const { writeFileSync } = require('node:fs');

test('test ran from cwd', () => {
  writeFileSync(${JSON.stringify(marker)}, 'ran');
});
`);

const controller = new AbortController();
const stream = run({
  cwd: watchedDir,
  watch: true,
  signal: controller.signal,
  isolation: 'none',
}).on('data', function({ type }) {
  if (type === 'test:watch:drained') {
    stream.removeAllListeners('test:fail');
    stream.removeAllListeners('test:pass');
    controller.abort();
  }
});

stream.on('test:fail', common.mustNotCall());
stream.on('test:pass', common.mustCall(1));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);

assert.strictEqual(readFileSync(marker, 'utf8'), 'ran');
