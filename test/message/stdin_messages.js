'use strict';

require('../common');

const spawn = require('child_process').spawn;

function run(cmd, strict, cb) {
  const args = [];
  if (strict) args.push('--use_strict');
  args.push('-p');
  const child = spawn(process.execPath, args);
  child.stdout.pipe(process.stdout);
  child.stderr.pipe(process.stdout);
  child.stdin.end(cmd);
  child.on('close', cb);
}

const queue =
  [ 'with(this){__filename}',
    '42',
    'throw new Error("hello")',
    'var x = 100; y = x;',
    'var ______________________________________________; throw 10' ];

function go() {
  const c = queue.shift();
  if (!c) return console.log('done');
  run(c, false, function() {
    run(c, true, go);
  });
}

go();
