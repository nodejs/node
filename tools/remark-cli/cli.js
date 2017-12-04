#!/usr/bin/env node
'use strict';

var start = require('unified-args');
var extensions = require('markdown-extensions');
var processor = require('remark');
var proc = require('remark/package.json');
var cli = require('./package.json');

start({
  processor: processor,
  name: proc.name,
  description: cli.description,
  version: [
    proc.name + ': ' + proc.version,
    cli.name + ': ' + cli.version
  ].join(', '),
  pluginPrefix: proc.name,
  presetPrefix: proc.name + '-preset',
  packageField: proc.name + 'Config',
  rcName: '.' + proc.name + 'rc',
  ignoreName: '.' + proc.name + 'ignore',
  extensions: extensions
});
