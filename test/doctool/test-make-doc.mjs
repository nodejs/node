import * as common from '../common/index.mjs';

import assert from 'assert';
import fs from 'fs';
import path from 'path';

if (common.isWindows) {
  common.skip('`make doc` does not run on Windows');
}

// This tests that `make doc` generates the documentation properly.
// Note that for this test to pass, `make doc` must be run first.

const apiURL = new URL('../../out/doc/api/', import.meta.url);
const mdURL = new URL('../../doc/api/', import.meta.url);
const allMD = fs.readdirSync(mdURL);
const allDocs = fs.readdirSync(apiURL);
assert.ok(allDocs.includes('index.html'));

const actualDocs = allDocs.filter(
  (name) => {
    const extension = path.extname(name);
    return extension === '.html' || extension === '.json';
  }
);

for (const name of actualDocs) {
  if (name.startsWith('all.')) continue;

  assert.ok(
    allMD.includes(name.replace(/\.\w+$/, '.md')),
    `Unexpected output: out/doc/api/${name}, remove and rerun.`
  );
}

const toc = fs.readFileSync(new URL('./index.html', apiURL), 'utf8');
const re = /href="([^/]+\.html)"/;
const globalRe = new RegExp(re, 'g');
const links = toc.match(globalRe);
assert.notStrictEqual(links, null);

// Filter out duplicate links, leave just filenames, add expected JSON files.
const linkedHtmls = [...new Set(links)].map((link) => link.match(re)[1])
                      .concat(['index.html']);
const expectedJsons = linkedHtmls
                       .map((name) => name.replace('.html', '.json'));
const expectedDocs = linkedHtmls.concat(expectedJsons);

// Test that all the relative links in the TOC match to the actual documents.
for (const expectedDoc of expectedDocs) {
  assert.ok(actualDocs.includes(expectedDoc), `${expectedDoc} does not exist`);
}

// Test that all the actual documents match to the relative links in the TOC
// and that they are not empty files.
for (const actualDoc of actualDocs) {
  assert.ok(
    expectedDocs.includes(actualDoc), `${actualDoc} does not match TOC`);

  assert.notStrictEqual(
    fs.statSync(new URL(`./${actualDoc}`, apiURL)).size,
    0,
    `${actualDoc} is empty`
  );
}
