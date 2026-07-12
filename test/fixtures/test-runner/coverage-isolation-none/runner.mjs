import { run } from 'node:test';
import { join } from 'node:path';

const stream = run({
  files: [join(import.meta.dirname, 'tests', 'foo.test.mjs')],
  coverage: true,
  isolation: 'none',
  cwd: import.meta.dirname,
});
stream.on('test:fail', () => process.exit(10));
let summary;
stream.on('test:coverage', (event) => { summary = event.summary; });
for await (const _ of stream);
if (!summary || summary.files.length === 0) process.exit(11);
const hasSrc = summary.files.some((f) => f.path.endsWith('foo.mjs') && !f.path.endsWith('foo.test.mjs'));
const hasTest = summary.files.some((f) => f.path.endsWith('foo.test.mjs'));
if (!hasSrc) process.exit(12);
if (hasTest) process.exit(13);