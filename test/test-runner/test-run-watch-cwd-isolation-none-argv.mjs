// Test run({ watch: true, cwd, isolation: 'none' }) does not reuse the
// parent process argv when spawning the watch child.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { run } from 'node:test';
import fixtures from '../common/fixtures.js';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

const passed = [];
const controller = new AbortController();
const stream = run({
  // Avoid delayed file creation notifications triggering a watch restart.
  cwd: fixtures.path('test-runner-watch'),
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
stream.on('test:pass', common.mustCall((data) => passed.push(data.name), 1));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);

// Validate the expected test ran by name:
assert.deepStrictEqual(passed, ['test has ran']);
