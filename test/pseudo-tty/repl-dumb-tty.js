'use strict';
const common = require('../common');

process.env.TERM = 'dumb';

const repl = require('repl');
const ArrayStream = require('../common/arraystream');

repl.start('> ');
process.stdin.push('conso'); // No completion preview.
process.stdin.push('le.log("foo")\n');
process.stdin.push('1 + 2'); // No input preview.
process.stdin.push('\n');
process.stdin.push('"str"\n');
process.stdin.push('console.dir({ a: 1 })\n');
process.stdin.push('{ a: 1 }\n');
process.stdin.push('\n');
process.stdin.push('.exit\n');

// Verify <ctrl> + D support.
{
  const stream = new ArrayStream();
  const replServer = new repl.REPLServer({
    prompt: '> ',
    terminal: true,
    input: stream,
    output: process.stdout,
    useColors: false
  });

  replServer.on('close', common.mustCall());
  // Verify that <ctrl> + R or <ctrl> + C does not trigger the reverse search.
  replServer.write(null, { ctrl: true, name: 'r' });
  replServer.write(null, { ctrl: true, name: 's' });
  replServer.write(null, { ctrl: true, name: 'd' });
}
