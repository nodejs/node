'use strict';

// To aid in future maintenance, this layout closely matches remark-cli/cli.js.
// https://github.com/remarkjs/remark/blob/master/packages/remark-cli/cli.js

const start = require('unified-args');
const extensions = require('markdown-extensions');
const processor = require('remark');
const proc = require('remark/package.json');
const cli = require('../package.json');
const lintNode = require('remark-preset-lint-node');

start({
  processor: processor().use(lintNode),
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
