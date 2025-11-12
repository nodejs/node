'use strict';
const common = require('../common');
const { startNewREPLServer } = require('../common/repl');

const { replServer } = startNewREPLServer();

replServer.setupHistory('/nonexistent/file', common.mustSucceed(() => {
  replServer.close();
}));
