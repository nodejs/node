// Test run({ watch: true }) emits test:watch:restarted when file is updated
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

let alreadyDrained = false;
const events = [];
const testWatchRestarted = common.mustCall(1);

const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
  watch: true,
  signal: controller.signal,
}).on('data', function({ type }) {
  events.push(type);
  if (type === 'test:watch:restarted') {
    testWatchRestarted();
  }
  if (type === 'test:watch:drained') {
    if (alreadyDrained) {
      controller.abort();
    }
    alreadyDrained = true;
  }
});

await once(stream, 'test:watch:drained');

writeFileSync(join(tmpdir.path, 'test.js'), fixtureContent['test.js']);

// eslint-disable-next-line no-unused-vars
for await (const _ of stream);

assert.partialDeepStrictEqual(events, [
  'test:watch:drained',
  'test:watch:restarted',
  'test:watch:drained',
]);
