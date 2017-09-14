'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const spawn = require('child_process').spawn;

const proc = spawn(process.execPath, ['inspect', 'foo']);
proc.stdout.setEncoding('utf8');

let output = '';
proc.stdout.on('data', (data) => {
  output += data;
  if (output.includes('debug> '))
    proc.stdin.write('.exit\n');
});
