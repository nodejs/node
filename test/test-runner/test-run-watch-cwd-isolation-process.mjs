// Test run({ watch: true, cwd, isolation: 'process' }) runs with different
// cwd while in watch mode and isolation process
import * as common from '../common/index.mjs';
import { run } from 'node:test';
import { writeFileSync, readFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import { join } from 'node:path';
import fixtures from '../common/fixtures.js';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

tmpdir.refresh();

const testJs = fixtures.path('test-runner-watch', 'test.js');
const testJsContent = readFileSync(testJs, 'utf8');
writeFileSync(join(tmpdir.path, 'test.js'), testJsContent);
writeFileSync(join(tmpdir.path, 'dependency.js'), 'module.exports = {};');
writeFileSync(join(tmpdir.path, 'dependency.mjs'), 'export const a = 1;');

const controller = new AbortController();
const stream = run({
  cwd: tmpdir.path,
  watch: true,
  signal: controller.signal,
  isolation: 'process',
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
