import fs from 'fs';
import child_process from 'node:child_process';
import { resolve, join } from 'node:path';

import { unified } from 'unified';
import remarkParse from 'remark-parse';
import remarkStringify from 'remark-stringify';
import presetLintNode from 'remark-preset-lint-node';
import { read } from 'to-vfile';
import { reporter } from 'vfile-reporter';

const paths = process.argv.slice(2);

if (!paths.length) {
  console.error('Usage: lint-md.mjs <path> [<path> ...]');
  process.exit(1);
}

let format = false;

if (paths[0] === '--format') {
  paths.shift();
  format = true;
}

const linter = unified()
  .use(remarkParse)
  .use(presetLintNode)
  .use(remarkStringify);

const rootDir = resolve(import.meta.dirname, '..', '..');
const npxPath = join(rootDir, 'deps', 'npm', 'bin', 'npx-cli.js');
const cliPath = resolve(join(rootDir, 'doc', 'api', 'cli.md'));
const manFilePath = join(rootDir, 'doc', 'node.1');

function handleDifference(filePath) {
  process.exitCode = 1;
  const buildCmd = process.platform === 'win32' ? 'vcbuild' : 'make';
  console.error(`${filePath} is not formatted. Please run '${buildCmd} format-md'.`);
}

paths.forEach(async (path) => {
  const file = await read(path);
  // We need to calculate `fileContents` before running `linter.process(files)`
  // because `linter.process(files)` mutates `file` and returns it as `result`.
  // So we won't be able to use `file` after that to see if its contents have
  // changed as they will have been altered to the changed version.
  const fileContents = file.toString();
  const result = await linter.process(file);
  const isDifferent = fileContents !== result.toString();
  let manPageContent;
  let isManPageModified = false;
  if (resolve(path) === cliPath) {
    try {
      child_process.execFileSync(process.execPath, [
        npxPath,
        '--yes',
        'github:nodejs/api-docs-tooling',
        '-i', path,
        '-o', '.tmp.1',
        '-t', 'man-page',
      ]);
      manPageContent = fs.readFileSync('.tmp.1', 'utf-8');
      isManPageModified = manPageContent !== fs.readFileSync(manFilePath, 'utf-8');
      fs.rmSync('.tmp.1');
    } catch {
      console.warn('Failed to lint man-page.');
    }
  }

  if (format) {
    if (isDifferent) {
      fs.writeFileSync(path, result.toString());
    }
    if (isManPageModified) {
      fs.writeFileSync(manFilePath, manPageContent);
    }
  } else {
    if (isDifferent) handleDifference(path);
    if (isManPageModified) handleDifference('doc/node.1');
    if (result.messages.length) {
      process.exitCode = 1;
      console.error(reporter(result));
    }
  }
});
