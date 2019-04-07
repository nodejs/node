'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

// Adding an `uncaughtException` listener in an REPL instance suppresses errors
// in the whole application.
// Closing the instance won't remove those listeners either.

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
