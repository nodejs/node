import fs from 'fs';
import { extname, join, resolve } from 'path';
import { pathToFileURL } from 'url';

import gfm from 'remark-gfm';
import markdown from 'remark-parse';
import unified from 'unified';

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
      entry.name !== 'build' &&
      entry.name !== 'changelogs' &&
      entry.name !== 'deps' &&
      entry.name !== 'fixtures' &&
      entry.name !== 'gyp' &&
      entry.name !== 'node_modules' &&
      entry.name !== 'out' &&
      entry.name !== 'tmp'
    ) {
      findMarkdownFilesRecursively(path);
    } else if (entry.isFile() && extname(entry.name) === '.md') {
      checkFile(path);
    }
  }
}

function checkFile(path) {
  const tree = unified()
    .use(markdown)
    .use(gfm)
    .parse(fs.readFileSync(path));

  const base = pathToFileURL(path);
  let previousDefinitionLabel;
  for (const node of getLinksRecursively(tree)) {
    const targetURL = new URL(node.url, base);
    if (targetURL.protocol === 'file:' && !fs.existsSync(targetURL)) {
      const { line, column } = node.position.start;
      console.error((process.env.GITHUB_ACTIONS ?
        `::error file=${path},line=${line},col=${column}::` : '') +
        `Broken link at ${path}:${line}:${column} (${node.url})`);
      process.exitCode = 1;
    }
    if (node.type === 'definition') {
      if (previousDefinitionLabel &&
          previousDefinitionLabel > node.label) {
        const { line, column } = node.position.start;
        console.error(
          (process.env.GITHUB_ACTIONS ? `::error file=${path},line=${line},col=${column}::` : '') +
          `Unordered reference at ${path}:${line}:${column} ("${node.label}" should be before "${previousDefinitionLabel}")`
        );
        process.exitCode = 1;
      }
      previousDefinitionLabel = node.label;
    }
  }
}
