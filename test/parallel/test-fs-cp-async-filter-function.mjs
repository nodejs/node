// This tests that cp() applies filter function.

import { mustCall } from '../common/index.mjs';
import { nextdir, collectEntries } from '../common/fs.js';
import assert from 'node:assert';
import { cp, statSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cp(src, dest, {
  filter: (path) => {
    const pathStat = statSync(path);
    return pathStat.isDirectory() || path.endsWith('.js');
  },
  dereference: true,
  recursive: true,
}, mustCall((err) => {
  assert.strictEqual(err, null);
  const destEntries = [];
  collectEntries(dest, destEntries);
  for (const entry of destEntries) {
    assert.strictEqual(
      entry.isDirectory() || entry.name.endsWith('.js'),
      true
    );
  }
}));
