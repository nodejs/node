'use strict';
const common = require('../common');
process.noDeprecation = true;

if (process.argv[2] === 'child') {
  process.emitWarning('Something else is deprecated.', 'DeprecationWarning');
} else {
  // parent process
  const spawn = require('child_process').spawn;

  // spawn self as child
  const child = spawn(process.argv[0], [process.argv[1], 'child']);

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', common.mustNotCall());
}

