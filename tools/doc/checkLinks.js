'use strict';

const fs = require('fs');
const { Worker, isMainThread, workerData: path } = require('worker_threads');

function* getLinksRecursively(node) {
  if (
    (node.type === 'link' && !node.url.startsWith('#')) ||
    node.type === 'image'
  ) {
    yield node;
  }
  for (const child of node.children || []) {
    yield* getLinksRecursively(child);
  }
}

if (isMainThread) {
  const { extname, join, resolve } = require('path');
  const DIR = resolve(process.argv[2]);

  console.log('Running Markdown link checker...');

  async function* findMarkdownFilesRecursively(dirPath) {
    const fileNames = await fs.promises.readdir(dirPath);

    for (const fileName of fileNames) {
      const path = join(dirPath, fileName);

      const stats = await fs.promises.stat(path);
      if (
        stats.isDirectory() &&
        fileName !== 'api' &&
        fileName !== 'deps' &&
        fileName !== 'node_modules'
      ) {
        yield* findMarkdownFilesRecursively(path);
      } else if (extname(fileName) === '.md') {
        yield path;
      }
    }
  }

  function errorHandler(error) {
    console.error(error);
    process.exitCode = 1;
  }

  setImmediate(async () => {
    for await (const path of findMarkdownFilesRecursively(DIR)) {
      new Worker(__filename, { workerData: path }).on('error', errorHandler);
    }
  });
} else {
  const unified = require('unified');
  const { pathToFileURL } = require('url');

  const tree = unified()
    .use(require('remark-parse'))
    .parse(fs.readFileSync(path));

  const base = pathToFileURL(path);
  for (const node of getLinksRecursively(tree)) {
    const targetURL = new URL(node.url, base);
    if (targetURL.protocol === 'file:' && !fs.existsSync(targetURL)) {
      const error = new Error('Broken link in a Markdown document.');
      const { start } = node.position;
      error.stack =
        error.stack.substring(0, error.stack.indexOf('\n') + 5) +
        `at ${node.type} (${path}:${start.line}:${start.column})`;
      throw error;
    }
  }
}
