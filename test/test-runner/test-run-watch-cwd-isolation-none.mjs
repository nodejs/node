// Test run({ watch: true, cwd, isolation: 'none' }) runs with different cwd while in watch mode and isolation none
import * as common from '../common/index.mjs';
import { run } from 'node:test';
import fixtures from '../common/fixtures.js';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

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
stream.on('test:pass', common.mustCall(1));
// eslint-disable-next-line no-unused-vars
for await (const _ of stream);
