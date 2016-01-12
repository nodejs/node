'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var args = ['-i'];
var child = spawn(process.execPath, args);

var input = '(function(){"use strict"; const y=1;y=2})()\n';
var expectedOutput = common.engineSpecificMessage({
  v8 : /^> TypeError: Assignment to constant variable.\n/,
  chakracore : /^> SyntaxError: Assignment to const\n/
});

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  throw new Error('child.stderr be silent');
});

child.stdout.setEncoding('utf8');
var out = '';
child.stdout.on('data', function(c) {
  out += c;
});
child.stdout.on('end', function() {
  assert(expectedOutput.test(out));
  console.log('ok');
});

child.stdin.end(input);
