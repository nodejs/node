'use strict';

const engine = require('unified-engine');
const options = require('unified-args/lib/options');
const extensions = require('markdown-extensions');
const processor = require('remark');
const proc = require('remark/package.json');
const cli = require('../package.json');
const { plugins } = require('remark-preset-lint-node');

const args = {
  processor: processor,
  name: proc.name,
  description: cli.description,
  version: [
    proc.name + ': ' + proc.version,
    cli.name + ': ' + cli.version
  ].join(', '),
  ignoreName: '.' + proc.name + 'ignore',
  extensions: extensions
};
const config = options(process.argv.slice(2), args);
config.detectConfig = false;
config.plugins = plugins;

engine(config, function done(err, code) {
  if (err) console.error(err);
  process.exit(code);
});
