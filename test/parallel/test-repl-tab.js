'use strict';
const common = require('../common');
const { startNewREPLServer } = require('../common/repl');

const { replServer } = startNewREPLServer({
  eval: function(cmd, context, filename, callback) {
    callback(null, cmd);
  },
});

replServer.complete('', common.mustSucceed());
