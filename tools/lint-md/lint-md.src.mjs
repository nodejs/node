import fs from 'fs';

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

paths.forEach(async (path) => {
  const file = await read(path);
  const result = await linter.process(file);
  if (format) {
    fs.writeFileSync(path, result.toString());
  } else if (result.messages.length) {
    process.exitCode = 1;
    console.error(reporter(result));
  }
});
