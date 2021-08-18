// To aid in future maintenance, this layout closely matches remark-cli/cli.js.
// https://github.com/remarkjs/remark/blob/HEAD/packages/remark-cli/cli.js

import { args } from 'unified-args';
import extensions from 'markdown-extensions';
import { remark } from 'remark';
import proc from 'remark/package.json';
import cli from '../package.json';
import lintNode from 'remark-preset-lint-node';
import gfm from 'remark-gfm';

args({
  processor: remark().use(gfm).use(lintNode),
  name: proc.name,
  description: cli.description,
  version: [
    proc.name + ': ' + proc.version,
    cli.name + ': ' + cli.version,
  ].join(', '),
  pluginPrefix: proc.name,
  presetPrefix: proc.name + '-preset',
  packageField: proc.name + 'Config',
  rcName: '.' + proc.name + 'rc',
  ignoreName: '.' + proc.name + 'ignore',
  extensions: extensions,
  detectConfig: false,
});
