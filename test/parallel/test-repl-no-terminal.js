'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

const stream = new ArrayStream();

const replServer = repl.start({ terminal: false, input: stream, output: stream });

replServer.setupHistory('/nonexistent/file', common.mustSucceed(() => {
  replServer.close();
}));
