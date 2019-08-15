'use strict';
const common = require('../common');

process.env.TERM = 'dumb';

const repl = require('repl');
const ArrayStream = require('../common/arraystream');

repl.start('> ');
process.stdin.push('console.log("foo")\n');
process.stdin.push('1 + 2\n');
process.stdin.push('"str"\n');
process.stdin.push('console.dir({ a: 1 })\n');
process.stdin.push('{ a: 1 }\n');
process.stdin.push('\n');
process.stdin.push('.exit\n');

// Verify Control+D support.
{
  const stream = new ArrayStream();
  const replServer = repl.start({
    prompt: '> ',
    terminal: true,
    input: stream,
    output: stream,
    useColors: false
  });

  replServer.on('close', common.mustCall());
  replServer.write(null, { ctrl: true, name: 'd' });
}
