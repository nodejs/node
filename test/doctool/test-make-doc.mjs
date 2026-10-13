import * as common from '../common/index.mjs';

import assert from 'assert';
import fs from 'fs';
import path from 'path';

if (common.isWindows) {
  common.skip('`make doc` does not run on Windows');
}

// This tests that `make doc` generates the documentation properly: an HTML
// page and a JSON file for every Markdown file under `doc/api`, and nothing
// for anything else. Note that for this test to pass, `make doc` must be run
// first.

const apiURL = new URL('../../out/doc/api/', import.meta.url);
const mdURL = new URL('../../doc/api/', import.meta.url);

// Kept in sync with `skip_apidoc_files` in the Makefile.
const skippedDocs = ['quic'];

const sources = fs.readdirSync(mdURL)
  .filter((name) => path.extname(name) === '.md')
  .map((name) => path.basename(name, '.md'));
const generated = fs.readdirSync(apiURL);

// These have no Markdown source of their own.
const expectedExtras = ['index.html', 'all.html', 'all.json'];
for (const name of expectedExtras) {
  assert.ok(generated.includes(name), `${name} was not generated`);
}

for (const name of sources) {
  for (const extension of ['.html', '.json']) {
    const file = `${name}${extension}`;
    if (skippedDocs.includes(name)) {
      assert.ok(!generated.includes(file), `${file} was unexpectedly generated`);
      continue;
    }
    assert.ok(generated.includes(file), `${file} was not generated`);
    assert.notStrictEqual(
      fs.statSync(new URL(`./${file}`, apiURL)).size,
      0,
      `${file} is empty`,
    );
  }
}

// Every generated page must come from a current Markdown file, so a stale
// `out/doc/api` (e.g. from a renamed document) is noticed.
const expectedPages = ['404.html', 'apilinks.json', ...expectedExtras];
for (const name of generated) {
  const extension = path.extname(name);
  if (extension !== '.html' && extension !== '.json') continue;
  if (expectedPages.includes(name)) continue;
  assert.ok(
    sources.includes(path.basename(name, extension)),
    `Unexpected output: out/doc/api/${name}, remove and rerun.`,
  );
}
