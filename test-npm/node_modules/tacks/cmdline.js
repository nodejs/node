#!/usr/bin/env node
'use strict'
var argv = require('yargs')
  .usage('Usage: $0 fixturedir')
  .demand(1, 1)
  .argv
var generateFromDir = require('./generate-from-dir.js')

var fixturedata = generateFromDir(argv._[0])
console.log(fixturedata)
