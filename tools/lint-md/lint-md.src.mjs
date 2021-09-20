import { unified } from 'unified';
import remarkParse from 'remark-parse';
import remarkStringify from 'remark-stringify';
import presetLintNode from 'remark-preset-lint-node';
import gfm from 'remark-gfm';
import { read } from 'to-vfile';
import { reporter } from 'vfile-reporter';

const paths = process.argv.slice(2);

const linter = unified()
  .use(remarkParse)
  .use(gfm)
  .use(presetLintNode)
  .use(remarkStringify);

paths.forEach(async (path) => {
  const file = await read(path);
  const result = await linter.process(file);
  if (result.messages.length) {
    process.exitCode = 1;
    console.error(reporter(result));
  }
  // TODO: allow reformatting by writing `String(result)` to the input file
});
