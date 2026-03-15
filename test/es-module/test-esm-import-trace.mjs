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
  assert.match(result.stderr, /Imported by:/);
  assert.match(result.stderr, /.*foo\.mjs:1:8/);
  assert.match(result.stderr, /.*entry\.mjs:1:8/);
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
  const traceFoo = /.*foo\.mjs:1:8/;
  const traceAlt = /.*alt\.mjs:1:8/;
  const entryFoo = /.*entry\.mjs:1:8/;
  const entryAlt = /.*entry\.mjs:2:8/;

  let parentMatched;
  if (traceFoo.test(result.stderr)) {
    parentMatched = 'foo';
    assert(entryFoo.test(result.stderr),
      'Import trace should show foo.mjs imported by entry.mjs with line/column');
  } else if (traceAlt.test(result.stderr)) {
    parentMatched = 'alt';
    assert(entryAlt.test(result.stderr),
      'Import trace should show alt.mjs imported by entry.mjs with line/column');
  } else {
    assert.fail('Import trace should show either foo.mjs or alt.mjs as parent with line/column');
  }
});