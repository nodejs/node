// Test run({ watch: true, cwd, isolation: 'none' }) runs with different cwd while in watch mode and isolation none
import * as common from '../common/index.mjs';
import { run } from 'node:test';
import tmpdir from '../common/tmpdir.js';
import { refreshForTestRunnerWatch, skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
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
