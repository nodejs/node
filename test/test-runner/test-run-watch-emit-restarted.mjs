// Test run({ watch: true }) emits test:watch:restarted when file is updated
import '../common/index.mjs';
import { run } from 'node:test';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { once } from 'node:events';
import tmpdir from '../common/tmpdir.js';
import { refreshForTestRunnerWatch, skipIfNoWatch, fixtureContent } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

const events = [];
let written = false;

const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
  watch: true,
  signal: controller.signal,
}).on('data', function({ type }) {
  if (type !== 'test:watch:restarted' && type !== 'test:watch:drained') {
    return;
  }
  events.push(type);
  // Watchers with latency (FSEvents) can still report the fixture setup after
  // the first run has started, so only a restart after the write below counts.
  if (written && type === 'test:watch:drained' && events.at(-2) === 'test:watch:restarted') {
    controller.abort();
  }
});

await once(stream, 'test:watch:drained');

events.length = 0;
written = true;
writeFileSync(join(tmpdir.path, 'test.js'), fixtureContent['test.js']);

// eslint-disable-next-line no-unused-vars
for await (const _ of stream);

assert.deepStrictEqual(events.slice(-2), ['test:watch:restarted', 'test:watch:drained']);
