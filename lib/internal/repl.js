'use strict';

const {
  Number,
  NumberIsNaN,
  NumberParseInt,
} = primordials;

const REPL = require('repl');
const { kStandaloneREPL } = require('internal/repl/utils');

module.exports = { __proto__: REPL };
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
    ...opts,
  };

  if (NumberParseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }

  if (env.NODE_REPL_MODE) {
    opts.replMode = {
      'strict': REPL.REPL_MODE_STRICT,
      'sloppy': REPL.REPL_MODE_SLOPPY,
    }[env.NODE_REPL_MODE.toLowerCase().trim()];
  }

  if (opts.replMode === undefined) {
    opts.replMode = REPL.REPL_MODE_SLOPPY;
  }

  const size = Number(env.NODE_REPL_HISTORY_SIZE);
  if (!NumberIsNaN(size) && size > 0) {
    opts.size = size;
  } else {
    opts.size = 1000;
  }

  const term = 'terminal' in opts ? opts.terminal : process.stdout.isTTY;
  opts.filePath = term ? env.NODE_REPL_HISTORY : '';

  const repl = REPL.start(opts);

  repl.setupHistory({
    filePath: opts.filePath,
    size: opts.size,
    onHistoryFileLoaded: cb,
  });
}
