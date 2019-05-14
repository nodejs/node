'use strict';
require('../common');

process.env.TERM = 'dumb';

const repl = require('repl');

repl.start('> ');
process.stdin.push('console.log("foo")\n');
process.stdin.push('1 + 2\n');
process.stdin.push('"str"\n');
process.stdin.push('console.dir({ a: 1 })\n');
process.stdin.push('{ a: 1 }\n');
process.stdin.push('\n');
process.stdin.push('.exit\n');
