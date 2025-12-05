import fs from 'node:fs';
import { parseArgs } from 'node:util';

import { remark } from 'remark';
import remarkLintApi from '@node-core/remark-lint/api';
import remarkLintBase from '@node-core/remark-lint';
import { read } from 'to-vfile';
import { reporter } from 'vfile-reporter';
import typeMap from '../../doc/api/type-map.json' with { type: 'json' };

const { values: { format }, positionals: paths } = parseArgs({
  options: {
    format: { type: 'boolean', default: false },
  },
  allowPositionals: true,
});

if (!paths.length) {
  console.error('Usage: lint-md.mjs [--format] <path> [<path> ...]');
  process.exit(1);
}

const apiLintProcessor = remarkLintApi({ typeMap, releasedVersions: process.env.NODE_RELEASED_VERSIONS?.split(',') });
const apiLinter = remark().use(apiLintProcessor);
const baseLinter = remark().use(remarkLintBase).use({ settings: apiLintProcessor.settings });

paths.forEach(async (path) => {
  const file = await read(path);
  const linter = path.startsWith('doc/api/') ? apiLinter : baseLinter;

  // We need to calculate `fileContents` before running `linter.process(files)`
  // because `linter.process(files)` mutates `file` and returns it as `result`.
  // So we won't be able to use `file` after that to see if its contents have
  // changed as they will have been altered to the changed version.
  const fileContents = file.toString();
  const result = await linter.process(file);
  const isDifferent = fileContents !== result.toString();

  if (result.messages.length) {
    process.exitCode = 1;
    console.error(reporter(result));
  }

  if (format) {
    if (isDifferent) {
      fs.writeFileSync(path, result.toString());
    }
  } else if (isDifferent) {
    process.exitCode = 1;
    const cmd = process.platform === 'win32' ? 'vcbuild' : 'make';
    console.error(`${path} is not formatted. Please run '${cmd} format-md'.`);
  }
});
