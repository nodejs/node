'use strict';
const { mustCall } = require('../common');
const { path } = require('../common/fixtures');

// This test ensures that the `'finish'` event is emitted on the child process's
// `stdout` and `stderr` when using `child_process.spawn()`.
// This is a regression test for https://github.com/nodejs/node/issues/19764.

const { spawn } = require('child_process');

const child = spawn(process.argv[0], [path('print-chars.js'), 10]);

child.stdout.on('finish', mustCall());
child.stdout.on('end', mustCall());
child.stdout.resume();

child.stderr.on('finish', mustCall());
child.stderr.on('end', mustCall());
child.stderr.resume();
