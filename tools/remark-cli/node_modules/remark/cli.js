#!/usr/bin/env node
var load = require('load-plugin');

var cli = load.resolve('remark-cli');

if (cli) {
  require(cli + '/cli');
} else {
  console.error([
    'Whoops, `remark` is mistakenly installed instead of `remark-cli`',
    '',
    '    npm uninstall remark',
    '    npm install remark-cli',
    '',
    'See https://git.io/vonyG for more information.'
  ].join('\n'));

  process.exit(1);
}
