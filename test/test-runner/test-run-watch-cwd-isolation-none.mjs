// Test run({ watch: true, cwd, isolation: 'none' }) runs with different cwd while in watch mode and isolation none
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { run } from 'node:test';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

const controller = new AbortController();
const stream = run({
  cwd: fixtures.path('test-runner-watch'),
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
stream.on('test:pass', common.mustCall());

// eslint-disable-next-line no-empty-pattern
for await (const {} of stream);
