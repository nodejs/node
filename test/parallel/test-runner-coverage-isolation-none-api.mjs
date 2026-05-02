import * as common from '../common/index.mjs';
import { before, describe, it, run } from 'node:test';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { cp, writeFile } from 'node:fs/promises';
import { join, sep } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

const skipIfNoInspector = {
  skip: !process.features.inspector ? 'inspector disabled' : false,
};

tmpdir.refresh();

async function setupFixtures() {
  const fixtureDir = fixtures.path('test-runner', 'coverage-isolation-none');
  await cp(fixtureDir, tmpdir.path, { recursive: true });
}

describe('run() coverage with isolation: none', skipIfNoInspector, () => {
  before(async () => {
    await setupFixtures();
  });

  for (const isolation of ['none', 'process']) {
    it(`reports src coverage and excludes test files by default (isolation=${isolation})`, async () => {
      const stream = run({
        files: [join(tmpdir.path, 'tests', 'foo.test.mjs')],
        coverage: true,
        isolation,
        cwd: tmpdir.path,
      });
      stream.on('test:fail', common.mustNotCall());

      let summary;
      stream.on('test:coverage', common.mustCall(({ summary: s }) => {
        summary = s;
      }));
      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);

      assert.ok(summary, 'test:coverage event must fire');
      const paths = summary.files.map((f) => f.path);
      assert.ok(
        paths.length > 0,
        `coverage files must be reported (isolation=${isolation}); got ${JSON.stringify(paths)}`,
      );
      assert.ok(
        paths.some((p) => p.endsWith(`src${sep}foo.mjs`)),
        `expected src/foo.mjs to be present (isolation=${isolation}); got ${JSON.stringify(paths)}`,
      );
      assert.ok(
        paths.every((p) => !p.endsWith('foo.test.mjs')),
        `expected foo.test.mjs to be excluded by default (isolation=${isolation}); got ${JSON.stringify(paths)}`,
      );
    });
  }

  it('is idempotent when --experimental-test-coverage is also passed', async () => {
    const runnerPath = join(tmpdir.path, 'runner.mjs');
    await writeFile(runnerPath, `\
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
`);
    const result = spawnSync(process.execPath, [
      '--experimental-test-coverage',
      runnerPath,
    ], { cwd: tmpdir.path });
    assert.strictEqual(
      result.status,
      0,
      `exited with ${result.status}\nstderr: ${result.stderr}\nstdout: ${result.stdout}`,
    );
  });
});
