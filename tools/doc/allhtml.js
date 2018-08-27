'use strict';

// Build all.html by combining the generated toc and apicontent from each
// of the generated html files.

const fs = require('fs');

const source = `${__dirname}/../../out/doc/api`;

// Get a list of generated API documents.
const htmlFiles = fs.readdirSync(source, 'utf8')
  .filter((name) => name.includes('.html') && name !== 'all.html');

// Read the table of contents.
const toc = fs.readFileSync(source + '/index.html', 'utf8');

// Extract (and concatenate) the toc and apicontent from each document.
let contents = '';
let apicontent = '';

// Identify files that should be skipped. As files are processed, they
// are added to this list to prevent dupes.
const seen = {
  'all.html': true,
  'index.html': true
};

for (const link of toc.match(/<a.*?>/g)) {
  const href = /href="(.*?)"/.exec(link)[1];
  if (!htmlFiles.includes(href) || seen[href]) continue;
  const data = fs.readFileSync(source + '/' + href, 'utf8');

  // Split the doc.
  const match = /(<\/ul>\s*)?<\/div>\s*<div id="apicontent">/.exec(data);

  contents += data.slice(0, match.index)
    .replace(/[\s\S]*?<div id="toc">\s*<h2>.*?<\/h2>\s*(<ul>\s*)?/, '');

  apicontent += data.slice(match.index + match[0].length)
    .replace(/(<\/div>\s*)*\s*<script[\s\S]*/, '')
    .replace(/<a href="(\w[^#"]*)#/g, (match, href) => {
      return htmlFiles.includes(href) ? '<a href="#' : match;
    })
    .trim() + '\n';

  // Mark source as seen.
  seen[href] = true;
}

// Replace various mentions of index with all.
let all = toc.replace(/index\.html/g, 'all.html')
  .replace('<a href="all.html" name="toc">', '<a href="index.html" name="toc">')
  .replace('index.json', 'all.json')
  .replace('api-section-index', 'api-section-all')
  .replace('data-id="index"', 'data-id="all"')
  .replace(/<li class="edit_on_github">.*?<\/li>/, '');

// Clean up the title.
all = all.replace(/<title>.*?\| /, '<title>');

// Insert the combined table of contents.
const tocStart = /<div id="toc">\s*<h2>.*?<\/h2>\s*/.exec(all);
all = all.slice(0, tocStart.index + tocStart[0].length) +
  '<ul>\n' + contents + '</ul>\n' +
  all.slice(tocStart.index + tocStart[0].length);

// Replace apicontent with the concatenated set of apicontents from each source.
const apiStart = /<div id="apicontent">\s*/.exec(all);
const apiEnd = /(\s*<\/div>)*\s*<script /.exec(all);
all = all.slice(0, apiStart.index + apiStart[0].length) +
  apicontent +
  all.slice(apiEnd.index);

// Write results.
fs.writeFileSync(source + '/all.html', all, 'utf8');

// Validate all hrefs have a target.
const ids = new Set();
const idRe = / id="(\w+)"/g;
let match;
while (match = idRe.exec(all)) {
  ids.add(match[1]);
}

const hrefRe = / href="#(\w+)"/g;
while (match = hrefRe.exec(all)) {
  if (!ids.has(match[1])) throw new Error(`link not found: ${match[1]}`);
}
