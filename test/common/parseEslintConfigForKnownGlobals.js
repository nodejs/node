#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const readline = require('readline');

const searchLines = [
  '  no-restricted-globals:',
  '  node-core/prefer-primordials:',
];

const restrictedGlobalLine = /^\s{4}- name:\s?([^#\s]+)/;
const closingLine = /^\s{0,3}[^#\s]/;

process.stdout.write('["process"');

const eslintConfig = readline.createInterface({
  input: fs.createReadStream(
    path.join(__dirname, '..', '..', 'lib', '.eslintrc.yaml')
  ),
  crlfDelay: Infinity,
});

let isReadingGlobals = false;
eslintConfig.on('line', (line) => {
  if (isReadingGlobals) {
    const match = restrictedGlobalLine.exec(line);
    if (match != null) {
      process.stdout.write(',' + JSON.stringify(match[1]));
    } else if (closingLine.test(line)) {
      isReadingGlobals = false;
    }
  } else if (line === searchLines[0]) {
    searchLines.shift();
    isReadingGlobals = true;
  }
});

eslintConfig.once('close', () => {
  process.stdout.write(']');
});
