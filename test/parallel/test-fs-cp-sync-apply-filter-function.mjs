// This tests that cpSync applies filter function.
import '../common/index.mjs';
import { nextdir, collectEntries } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, statSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cpSync(src, dest, {
  filter: (path) => {
    const pathStat = statSync(path);
    return pathStat.isDirectory() || path.endsWith('.js');
  },
  dereference: true,
  recursive: true,
});
const destEntries = [];
collectEntries(dest, destEntries);
for (const entry of destEntries) {
  assert.strictEqual(
    entry.isDirectory() || entry.name.endsWith('.js'),
    true
  );
}
