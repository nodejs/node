// Build all.html by combining the generated toc and apicontent from each
// of the generated html files.

import fs from 'fs';

const source = new URL('../../out/doc/api/', import.meta.url);

// Get a list of generated API documents.
const htmlFiles = fs.readdirSync(source, 'utf8')
  .filter((name) => name.includes('.html') && name !== 'all.html');

// Read the table of contents.
const toc = fs.readFileSync(new URL('./index.html', source), 'utf8');

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
  const data = fs.readFileSync(new URL(`./${href}`, source), 'utf8');

  // Split the doc.
  const match = /(<\/ul>\s*)?<\/\w+>\s*<\w+ id="apicontent">/.exec(data);

  // Get module name
  const moduleName = href.replace(/\.html$/, '');

  contents += data.slice(0, match.index)
    .replace(/[\s\S]*?id="toc"[^>]*>\s*<\w+>.*?<\/\w+>\s*(<ul>\s*)?/, '')
    // Prefix TOC links with current module name
    .replace(/<a href="#(?!DEP[0-9]{4})([^"]+)"/g, (match, anchor) => {
      return `<a href="#${moduleName}_${anchor}"`;
    });

  apicontent += '<section>' + data.slice(match.index + match[0].length)
    .replace(/<!-- API END -->[\s\S]*/, '</section>')
    // Prefix all in-page anchor marks with module name
    .replace(/<a class="mark" href="#([^"]+)" id="([^"]+)"/g, (match, anchor, id) => {
      if (anchor !== id) throw new Error(`Mark does not match: ${anchor} should match ${id}`);
      return `<a class="mark" href="#${moduleName}_${anchor}" id="${moduleName}_${anchor}"`;
    })
    // Prefix all in-page links with current module name
    .replace(/<a href="#(?!DEP[0-9]{4})([^"]+)"/g, (match, anchor) => {
      return `<a href="#${moduleName}_${anchor}"`;
    })
    // Prefix all links to other docs modules with those module names
    .replace(/<a href="((\w[^#"]*)\.html)#/g, (match, href, linkModule) => {
      if (!htmlFiles.includes(href)) return match;
      return `<a href="#${linkModule}_`;
    })
    .trim() + '\n';

  // Mark source as seen.
  seen[href] = true;
}

// Replace various mentions of index with all.
let all = toc.replace(/index\.html/g, 'all.html')
  .replace('<a href="all.html">', '<a href="index.html">')
  .replace('index.json', 'all.json')
  .replace('api-section-index', 'api-section-all')
  .replace('data-id="index"', 'data-id="all"')
  .replace(/<li class="edit_on_github">.*?<\/li>/, '');

// Clean up the title.
all = all.replace(/<title>.*?\| /, '<title>');

// Insert the combined table of contents.
const tocStart = /<!-- TOC -->/.exec(all);
all = all.slice(0, tocStart.index + tocStart[0].length) +
  '<details id="toc" open><summary>Table of contents</summary>\n' +
  '<ul>\n' + contents + '</ul>\n' +
  '</details>\n' +
  all.slice(tocStart.index + tocStart[0].length);

// Replace apicontent with the concatenated set of apicontents from each source.
const apiStart = /<\w+ id="apicontent">\s*/.exec(all);
const apiEnd = all.lastIndexOf('<!-- API END -->');
all = all.slice(0, apiStart.index + apiStart[0].length) +
  apicontent +
  all.slice(apiEnd);

// Write results.
fs.writeFileSync(new URL('./all.html', source), all, 'utf8');

// Validate all hrefs have a target.
const ids = new Set();
const idRe = / id="([^"]+)"/g;
let match;
while (match = idRe.exec(all)) {
  ids.add(match[1]);
}

const hrefRe = / href="#([^"]+)"/g;
while (match = hrefRe.exec(all)) {
  if (!ids.has(match[1])) throw new Error(`link not found: ${match[1]}`);
}
