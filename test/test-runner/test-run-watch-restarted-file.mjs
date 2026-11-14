// Test run({ watch: true }) reports the triggering file path in the
// test:watch:restarted event data.
import * as common from '../common/index.mjs';
import { run } from 'node:test';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { once } from 'node:events';
import tmpdir from '../common/tmpdir.js';
import { refreshForTestRunnerWatch, skipIfNoWatch, fixtureContent } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

const changedFile = join(tmpdir.path, 'test.js');

let alreadyDrained = false;
const restartedFiles = [];
const onRestarted = common.mustCall((file) => {
  restartedFiles.push(file);
}, 1);

const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
  watch: true,
  signal: controller.signal,
}).on('data', function({ type, data }) {
  if (type === 'test:watch:restarted') {
    onRestarted(data.file);
  }
  if (type === 'test:watch:drained') {
    if (alreadyDrained) {
      controller.abort();
    }
    alreadyDrained = true;
  }
});

await once(stream, 'test:watch:drained');

writeFileSync(changedFile, fixtureContent['test.js']);

// eslint-disable-next-line no-unused-vars
for await (const _ of stream);

assert.deepStrictEqual(restartedFiles, [changedFile]);
