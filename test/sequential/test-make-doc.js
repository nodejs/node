'use strict';

const common = require('../common');

const assert = require('assert');
const exec = require('child_process').exec;
const fs = require('fs');
const path = require('path');

if (common.isWindows) {
  common.skip('Do not make doc on Windows');
}

// Make sure all the relative links work and
// all the generated documents are linked in TOC
function verifyToc() {
  const apiPath = path.join(common.projectDir, 'out', 'doc', 'api');
  const docs = fs.readdirSync(apiPath);
  assert.notStrictEqual(docs.indexOf('_toc.html'), -1);

  const toc = fs.readFileSync(path.join(apiPath, '_toc.html'), 'utf8');
  const re = /href="([^/]+\.html)"/;
  const globalRe = new RegExp(re, 'g');
  const links = toc.match(globalRe);
  assert.notStrictEqual(links, null);

  const linkedHtmls = links.map((link) => link.match(re)[1]);
  for (const html of linkedHtmls) {
    assert.notStrictEqual(docs.indexOf(html), -1, `${html} does not exist`);
  }

  const excludes = ['.json', '_toc', 'assets'];
  const generatedHtmls = docs.filter(function(doc) {
    for (const exclude of excludes) {
      if (doc.includes(exclude)) {
        return false;
      }
    }
    return true;
  });

  for (const html of generatedHtmls) {
    assert.notStrictEqual(
      linkedHtmls.indexOf(html),
      -1,
      `${html} is not linked in toc`);
  }
}

exec('make doc', {
  cwd: common.projectDir
}, common.mustCall(function onExit(err, stdout, stderr) {
  console.log(stdout);
  if (stderr.length > 0) {
    console.log('stderr is not empty: ');
    console.log(stderr);
  }
  assert.ifError(err);
  verifyToc();
}));
