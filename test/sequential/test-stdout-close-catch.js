'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const child_process = require('child_process');
const fs = require('fs');

const testScript = path.join(common.fixturesDir, 'catch-stdout-error.js');

const cmd = JSON.stringify(process.execPath) + ' ' +
          JSON.stringify(testScript) + ' | ' +
          JSON.stringify(process.execPath) + ' ' +
          '-pe "process.exit(1);"';

const child = child_process.exec(cmd);
var output = '';
const outputExpect = { 'code': 'EPIPE',
                       'errno': 'EPIPE',
                       'syscall': 'write' };

child.stderr.on('data', function(c) {
  output += c;
});

child.on('close', function(code) {
  try {
    output = JSON.parse(output);
  } catch (er) {
    console.error(output);
    process.exit(1);
  }

  assert.deepEqual(output, outputExpect);
  console.log('ok');
});
