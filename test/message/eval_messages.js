'use strict';

require('../common');

var spawn = require('child_process').spawn;

function run(cmd, strict, cb) {
  var args = [];
  if (strict) args.push('--use_strict');
  args.push('-pe', cmd);
  var child = spawn(process.execPath, args);
  child.stdout.pipe(process.stdout);
  child.stderr.pipe(process.stdout);
  child.on('close', cb);
}

var queue =
  [ 'with(this){__filename}',
    '42',
    'throw new Error("hello")',
    'var x = 100; y = x;',
    'var ______________________________________________; throw 10' ];

function go() {
  var c = queue.shift();
  if (!c) return console.log('done');
  run(c, false, function() {
    run(c, true, go);
  });
}

go();
