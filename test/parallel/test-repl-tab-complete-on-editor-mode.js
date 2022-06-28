'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

const stream = new ArrayStream();
const replServer = repl.start({
  input: stream,
  output: stream,
  terminal: true,
});

// Editor mode
replServer.write('.editor\n');

// Regression test for https://github.com/nodejs/node/issues/43528
replServer.write('a');
replServer.write(null, { name: 'tab' }); // Should not throw

replServer.close();
