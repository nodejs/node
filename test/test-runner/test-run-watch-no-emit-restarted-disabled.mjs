// Test run({ watch: false }) does not emit test:watch:restarted
import * as common from '../common/index.mjs';
import { run } from 'node:test';
import { writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import { refreshForTestRunnerWatch, skipIfNoWatch, fixtureContent } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

const stream = run({
  cwd: tmpdir.path,
  watch: false,
});

stream.on('test:watch:restarted', common.mustNotCall());
writeFileSync(tmpdir.resolve('test.js'), fixtureContent['test.js']);

// eslint-disable-next-line no-unused-vars
for await (const _ of stream);
