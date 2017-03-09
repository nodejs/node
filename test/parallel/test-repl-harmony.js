'use strict';
require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;
const args = ['-i'];
const child = spawn(process.execPath, args);

const input = '(function(){"use strict"; const y=1;y=2})()\n';
const expectOut = /^> TypeError: Assignment to constant variable.\n/;

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  throw new Error('child.stderr be silent');
});

child.stdout.setEncoding('utf8');
let out = '';
child.stdout.on('data', function(c) {
  out += c;
});
child.stdout.on('end', function() {
  assert(expectOut.test(out));
  console.log('ok');
});

child.stdin.end(input);
