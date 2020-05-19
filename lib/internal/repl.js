'use strict';

const {
  Number,
  NumberIsNaN,
} = primordials;

const REPL = require('repl');
const { kStandaloneREPL } = require('internal/repl/utils');

function createInternalRepl(opts = {}, callback = () => {}) {
  opts = {
    [kStandaloneREPL]: true,
    useGlobal: true,
    breakEvalOnSigint: true,
    ...opts,
    terminal: parseInt(process.env.NODE_NO_READLINE) ? false : opts.terminal,
  };

  const historySize = Number(process.env.NODE_REPL_HISTORY_SIZE);
  if (!NumberIsNaN(historySize) && historySize > 0) {
    opts.historySize = historySize;
  } else {
    opts.historySize = 1000;
  }

  const repl = REPL.start(opts);
  const historyPath = repl.terminal ? process.env.NODE_REPL_HISTORY : '';
  repl.setupHistory(historyPath, (err, repl) => {
    if (err) {
      throw err;
    }
    callback(repl);
  });
}

module.exports = { createInternalRepl };
