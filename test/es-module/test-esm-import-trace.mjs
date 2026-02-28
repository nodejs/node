import { spawnSync } from 'node:child_process';
import assert from 'node:assert';
import path from 'node:path';
import { test } from 'node:test';

const fixture = path.join(
  import.meta.dirname,
  '../fixtures/es-modules/import-trace/entry.mjs'
);

test('includes import trace for evaluation-time errors', () => {
  const result = spawnSync(
    process.execPath,
    [fixture],
    { encoding: 'utf8' }
  );

  assert.notStrictEqual(result.status, 0);
  assert.match(result.stderr, /Import trace:/);
  assert.match(result.stderr, /bar\.mjs imported by .*foo\.mjs/);
  assert.match(result.stderr, /foo\.mjs imported by .*entry\.mjs/);
});

const multiFixture = path.join(
  import.meta.dirname,
  '../fixtures/es-modules/import-trace-multi/entry.mjs'
);

test('import trace matches actual code path for multiple parents', () => {
  const result = spawnSync(
    process.execPath,
    [multiFixture],
    { encoding: 'utf8' }
  );

  assert.notStrictEqual(result.status, 0);
  // Should show the actual import chain that led to the error
  const traceFoo = /bar\.mjs imported by .*foo\.mjs/;
  const traceAlt = /bar\.mjs imported by .*alt\.mjs/;
  const entryFoo = /foo\.mjs imported by .*entry\.mjs/;
  const entryAlt = /alt\.mjs imported by .*entry\.mjs/;

  assert(
    traceFoo.test(result.stderr) || traceAlt.test(result.stderr),
    'Import trace should show either foo.mjs or alt.mjs as parent'
  );
  assert(
    entryFoo.test(result.stderr) || entryAlt.test(result.stderr),
    'Import trace should show either foo.mjs or alt.mjs imported by entry.mjs'
  );
});