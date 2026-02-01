import { spawnSync } from 'node:child_process';
import assert from 'node:assert';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { test } from 'node:test';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const fixture = path.join(
  __dirname,
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