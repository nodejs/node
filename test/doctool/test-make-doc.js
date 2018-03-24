'use strict';
const common = require('../common');
if (common.isWindows) {
  common.skip('`make doc` does not run on Windows');
}

// This tests that `make doc` generates the documentation properly.
// Note that for this test to pass, `make doc` must be run first.

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const apiPath = path.resolve(__dirname, '..', '..', 'out', 'doc', 'api');
const allDocs = fs.readdirSync(apiPath);
assert.ok(allDocs.includes('_toc.html'));

const filter = ['assets', '_toc.html', '.md'];
const actualDocs = allDocs.filter(
  (name) => !filter.some((str) => name.includes(str))
);

const toc = fs.readFileSync(path.resolve(apiPath, '_toc.html'), 'utf8');
const re = /href="([^/]+\.html)"/;
const globalRe = new RegExp(re, 'g');
const links = toc.match(globalRe);
assert.notStrictEqual(links, null);

// Filter out duplicate links, leave just filenames, add expected JSON files.
const linkedHtmls = [...new Set(links)].map((link) => link.match(re)[1]);
const expectedJsons = linkedHtmls
                       .map((name) => name.replace('.html', '.json'))
                       .concat('_toc.json');
const expectedDocs = linkedHtmls.concat(expectedJsons);

// Test that all the relative links in the TOC match to the actual documents.
for (const expectedDoc of expectedDocs) {
  assert.ok(actualDocs.includes(expectedDoc), `${expectedDoc} does not exist`);
}

// Test that all the actual documents match to the relative links in the TOC.
for (const actualDoc of actualDocs) {
  assert.ok(
    expectedDocs.includes(actualDoc), `${actualDoc} does not not match TOC`);
}

// Test that all the actual documents are not empty files.
for (const actualDoc of actualDocs) {
  assert.ok(
    fs.statSync(path.join(apiPath, actualDoc)).size !== 0,
    `${actualDoc} is empty`
  );
}
