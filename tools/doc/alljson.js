'use strict';

// Build all.json by combining the miscs, modules, classes, globals, and methods
// from the generated json files.

const fs = require('fs');

const source = `${__dirname}/../../out/doc/api`;

// Get a list of generated API documents.
const jsonFiles = fs.readdirSync(source, 'utf8')
  .filter((name) => name.includes('.json') && name !== 'all.json');

// Read the table of contents.
const toc = fs.readFileSync(source + '/index.html', 'utf8');

// Initialize results. Only these four data values will be collected.
const results = {
  miscs: [],
  modules: [],
  classes: [],
  globals: [],
  methods: []
};

// Identify files that should be skipped. As files are processed, they
// are added to this list to prevent dupes.
const seen = {
  'all.json': true,
  'index.json': true
};

// Extract (and concatenate) the selected data from each document.
// Expand hrefs found in json to include source HTML file.
for (const link of toc.match(/<a.*?>/g)) {
  const href = /href="(.*?)"/.exec(link)[1];
  const json = href.replace('.html', '.json');
  if (!jsonFiles.includes(json) || seen[json]) continue;
  const data = JSON.parse(
    fs.readFileSync(source + '/' + json, 'utf8')
      .replace(/<a href=\\"#/g, `<a href=\\"${href}#`)
  );

  for (const property in data) {
    if (results.hasOwnProperty(property)) {
      results[property].push(...data[property]);
    }
  }

  // Mark source as seen.
  seen[json] = true;
}

// Write results.
fs.writeFileSync(source + '/all.json',
                 `${JSON.stringify(results, null, 2)}\n`, 'utf8');
