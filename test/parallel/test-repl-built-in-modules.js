'use strict';

require('../common');
const assert = require('assert');
const cp = require('child_process');

function runREPLWithAdditionalFlags(flags) {
  // Use -i to force node into interactive mode, despite stdout not being a TTY
  const args = ['-i'].concat(flags);
  const ret = cp.execFileSync(process.execPath, args, {
    input: 'require(\'events\');\nrequire(\'wasi\');',
    encoding: 'utf8',
  });
  return ret;
}

// Run REPL in normal mode.
let stdout = runREPLWithAdditionalFlags([]);
assert.match(stdout, /\[Function: EventEmitter\] {/);
assert.match(
  stdout,
  /Uncaught Error: Cannot find module 'wasi'[\w\W]+- <repl>\n/);

// Run REPL with '--experimental-wasi-unstable-preview1'
stdout = runREPLWithAdditionalFlags([
  '--experimental-wasi-unstable-preview1',
]);
assert.match(stdout, /\[Function: EventEmitter\] {/);
assert.doesNotMatch(
  stdout,
  /Uncaught Error: Cannot find module 'wasi'[\w\W]+- <repl>\n/);
assert.match(stdout, /{ WASI: \[class WASI\] }/);

{
  const res = cp.execFileSync(process.execPath, ['-i'], {
    input: "'wasi' in global",
    encoding: 'utf8',
  });
  // `wasi` shouldn't be defined on global when the flag is not set
  assert.match(res, /false\n/);
}
{
  const res = cp.execFileSync(process.execPath, ['-i', '--experimental-wasi-unstable-preview1'], {
    input: "'wasi' in global",
    encoding: 'utf8',
  });
  assert.match(res, /true\n/);
}
