'use strict';

const ArrayStream = require('../common/arraystream');
const repl = require('node:repl');

function startNewREPLServer(replOpts = {}) {
  const input = new ArrayStream();
  const output = new ArrayStream();

  output.accumulator = '';
  output.write = (data) => (output.accumulator += `${data}`.replaceAll('\r', ''));

  const replServer = repl.start({
    prompt: '',
    input,
    output,
    terminal: true,
    allowBlockingCompletions: true,
    ...replOpts,
  });

  return { replServer, input, output };
}

module.exports = { startNewREPLServer };
