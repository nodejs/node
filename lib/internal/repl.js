'use strict';

const REPL = require('repl');

module.exports = Object.create(REPL);
module.exports.createInternalRepl = createRepl;

function createRepl(env, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts;
    opts = null;
  }
  opts = {
    ignoreUndefined: false,
    terminal: process.stdout.isTTY,
    useGlobal: true,
    breakEvalOnSigint: true,
    ...opts
  };

  if (parseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }
  // The "dumb" special terminal, as defined by terminfo, doesn't support
  // ANSI color control codes.
  // see http://invisible-island.net/ncurses/terminfo.ti.html#toc-_Specials
  if (parseInt(env.NODE_DISABLE_COLORS) || env.TERM === 'dumb') {
    opts.useColors = false;
  }

  opts.replMode = {
    'strict': REPL.REPL_MODE_STRICT,
    'sloppy': REPL.REPL_MODE_SLOPPY
  }[String(env.NODE_REPL_MODE).toLowerCase().trim()];

  if (opts.replMode === undefined) {
    opts.replMode = REPL.REPL_MODE_SLOPPY;
  }

  const historySize = Number(env.NODE_REPL_HISTORY_SIZE);
  if (!Number.isNaN(historySize) && historySize > 0) {
    opts.historySize = historySize;
  } else {
    opts.historySize = 1000;
  }

  const repl = REPL.start(opts);
  repl.setupHistory(opts.terminal ? env.NODE_REPL_HISTORY : '', cb);
}
