'use strict';

const fs = require('fs');
const { extname, join, resolve } = require('path');
const unified = require('unified');
const { pathToFileURL } = require('url');
const DIR = resolve(process.argv[2]);

console.log('Running Markdown link checker...');
findMarkdownFilesRecursively(DIR);

function* getLinksRecursively(node) {
  if (node.url && !node.url.startsWith('#')) {
    yield node;
  }
  for (const child of node.children || []) {
    yield* getLinksRecursively(child);
  }
}

function findMarkdownFilesRecursively(dirPath) {
  const entries = fs.readdirSync(dirPath, { withFileTypes: true });

  for (const entry of entries) {
    const path = join(dirPath, entry.name);

    if (
      entry.isDirectory() &&
      entry.name !== 'api' &&
      entry.name !== 'fixtures' &&
      entry.name !== 'changelogs' &&
      entry.name !== 'deps' &&
      entry.name !== 'node_modules'
    ) {
      findMarkdownFilesRecursively(path);
    } else if (entry.isFile() && extname(entry.name) === '.md') {
      checkFile(path);
    }
  }
}

function checkFile(path) {
  const tree = unified()
    .use(require('remark-parse'))
    .parse(fs.readFileSync(path));

  const base = pathToFileURL(path);
  for (const node of getLinksRecursively(tree)) {
    const targetURL = new URL(node.url, base);
    if (targetURL.protocol === 'file:' && !fs.existsSync(targetURL)) {
      const { line, column } = node.position.start;
      console.error(`Broken link at ${path}:${line}:${column} (${node.url})`);
      process.exitCode = 1;
    }
  }
}
