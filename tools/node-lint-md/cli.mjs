// To aid in future maintenance, this layout closely matches remark-cli/cli.js.
// https://github.com/remarkjs/remark/blob/master/packages/remark-cli/cli.js

import { createRequire } from 'node:module';
import { args } from 'unified-args';
import { remark } from 'remark';
import lintNode from 'remark-preset-lint-node';
import gfm from 'remark-gfm';

const require = createRequire(import.meta.url);

const proc = require('remark/package.json');
const cli = require('./package.json');

args({
  processor: remark().use(gfm).use(lintNode),
  name: proc.name,
  description: cli.description,
  version: [
    proc.name + ': ' + proc.version,
    cli.name + ': ' + cli.version,
  ].join(', '),
  pluginPrefix: proc.name,
  packageField: proc.name + 'Config',
  rcName: '.' + proc.name + 'rc',
  ignoreName: '.' + proc.name + 'ignore',
  extensions: ['md']
});
