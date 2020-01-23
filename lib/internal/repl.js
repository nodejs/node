'use strict';

const {
  Number,
  NumberIsNaN,
  ObjectCreate,
} = primordials;

const REPL = require('repl');
const { kStandaloneREPL } = require('internal/repl/utils');

module.exports = ObjectCreate(REPL);
module.exports.createInternalRepl = createRepl;

function createRepl(env, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts;
    opts = null;
  }
  opts = {
    [kStandaloneREPL]: true,
    ignoreUndefined: false,
    useGlobal: true,
    breakEvalOnSigint: true,
    ...opts
  };

  if (parseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }

  if (env.NODE_REPL_MODE) {
    opts.replMode = {
      'strict': REPL.REPL_MODE_STRICT,
      'sloppy': REPL.REPL_MODE_SLOPPY
    }[env.NODE_REPL_MODE.toLowerCase().trim()];
  }

  if (opts.replMode === undefined) {
    opts.replMode = REPL.REPL_MODE_SLOPPY;
  }

  const historySize = Number(env.NODE_REPL_HISTORY_SIZE);
  if (!NumberIsNaN(historySize) && historySize > 0) {
    opts.historySize = historySize;
  } else {
    opts.historySize = 1000;
  }

  const repl = REPL.start(opts);
  const term = 'terminal' in opts ? opts.terminal : process.stdout.isTTY;
  repl.setupHistory(term ? env.NODE_REPL_HISTORY : '', cb);
}
