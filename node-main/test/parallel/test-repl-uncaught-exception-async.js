'use strict';

// This verifies that adding an `uncaughtException` listener in an REPL instance
// does not suppress errors in the whole application. Adding such listener
// should throw.

require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer, output } = startNewREPLServer(
  {
    prompt: '',
    terminal: false,
    useColors: false,
    global: false,
  },
  {
    disableDomainErrorAssert: true
  },
);

replServer.write(
  'process.nextTick(() => {\n' +
  '  process.on("uncaughtException", () => console.log("Foo"));\n' +
  '  throw new TypeError("foobar");\n' +
  '});\n'
);
replServer.write(
  'setTimeout(() => {\n' +
  '  throw new RangeError("abc");\n' +
  '}, 1);console.log()\n'
);

setTimeout(() => {
  replServer.close();
  const len = process.listenerCount('uncaughtException');
  process.removeAllListeners('uncaughtException');
  assert.strictEqual(len, 0);
  assert.match(output.accumulator, /ERR_INVALID_REPL_INPUT.*(?!Type)RangeError: abc/s);
}, 2);
