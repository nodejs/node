'use strict';

require('../common');

// Adding an `uncaughtException` listener in an REPL instance suppresses errors
// in the whole application.
// Closing the instance won't remove those listeners either.

if (process.argv[2] === 'child') {

  const ArrayStream = require('../common/arraystream');
  const repl = require('repl');

  let accum = '';

  const output = new ArrayStream();
  output.write = (data) => accum += data.replace('\r', '');

  const r = repl.start({
    prompt: '',
    input: new ArrayStream(),
    output,
    terminal: false,
    useColors: false,
    global: false
  });

  r.write(
    'setTimeout(() => {\n' +
    '  process.on("uncaughtException", () => console.log("Foo"));\n' +
    '}, 1);\n'
  );

  // Event listeners added to the global `process` won't be removed after
  // closing the REPL instance! Those should be removed again, especially since
  // the REPL's `global` option is set to `false`.
  r.close();

  setTimeout(() => {
    // This should definitely not be silenced!
    throw new Error('HU');
  }, 2);

  setTimeout(() => {
    console.error('FAILED');
    process.exit(1);
  }, 10);

  return;
}

const cp = require('child_process');
const assert = require('assert');

const res = cp.spawnSync(
  process.execPath,
  [__filename, 'child'],
  { encoding: 'utf8' }
);

assert.notStrictEqual(res.stderr, 'FAILED\n');
