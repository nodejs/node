'use strict';

require('../common');
const spawn = require('child_process').spawn;

const proc = spawn(process.execPath, ['debug', 'foo']);
proc.stdout.setEncoding('utf8');

let output = '';
proc.stdout.on('data', (data) => {
  output += data;
  if (output.includes('debug> '))
    proc.kill();
});
