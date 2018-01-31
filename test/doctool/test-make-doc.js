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
const docs = fs.readdirSync(apiPath);
assert.ok(docs.includes('_toc.html'));

const toc = fs.readFileSync(path.resolve(apiPath, '_toc.html'), 'utf8');
const re = /href="([^/]+\.html)"/;
const globalRe = new RegExp(re, 'g');
const links = toc.match(globalRe);
assert.notStrictEqual(links, null);

// Test that all the relative links in the TOC of the documentation
// work and all the generated documents are linked in TOC.
const linkedHtmls = links.map((link) => link.match(re)[1]);
for (const html of linkedHtmls) {
  assert.ok(docs.includes(html), `${html} does not exist`);
}

const excludes = ['.json', '.md', '_toc', 'assets'];
const generatedHtmls = docs.filter(function(doc) {
  for (const exclude of excludes) {
    if (doc.includes(exclude)) {
      return false;
    }
  }
  return true;
});

for (const html of generatedHtmls) {
  assert.ok(linkedHtmls.includes(html), `${html} is not linked in toc`);
}
