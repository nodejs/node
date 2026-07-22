'use strict';

const ArrayStream = require('../common/arraystream');
const repl = require('node:repl');
const assert = require('node:assert');

function startNewREPLServer(replOpts = {}, testingOpts = {}) {
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

  if (!testingOpts.disableDomainErrorAssert) {
    // Some errors are passed to the domain, but do not callback
    replServer._domain.on('error', assert.ifError);
  }

  return { replServer, input, output };
}

module.exports = { startNewREPLServer };
