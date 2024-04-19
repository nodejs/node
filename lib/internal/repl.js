'use strict';

const {
  Number,
  NumberIsNaN,
  NumberParseInt,
} = primordials;

const StableREPL = require('internal/repl/stable/index');
const ExperimentalREPL = require('internal/repl/experimental/index');
const { kStandaloneREPL } = require('internal/repl/stable/utils');
const {
  getOptionValue,
} = require('internal/options');

const useExperimentalRepl = getOptionValue('--experimental-repl') || false;

module.exports = { __proto__: useExperimentalRepl ? ExperimentalREPL : StableREPL };
module.exports.createInternalRepl = useExperimentalRepl ? createExperimentalRepl : createStableRepl;

function createStableRepl(env, opts, cb) {
  process.stdout.write(`Welcome to Node.js ${process.version}.\n` +
      'Type ".help" for more information.\n');

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
      'strict': StableREPL.REPL_MODE_STRICT,
      'sloppy': StableREPL.REPL_MODE_SLOPPY,
    }[env.NODE_REPL_MODE.toLowerCase().trim()];
  }

  if (opts.replMode === undefined) {
    opts.replMode = StableREPL.REPL_MODE_SLOPPY;
  }

  const historySize = Number(env.NODE_REPL_HISTORY_SIZE);
  if (!NumberIsNaN(historySize) && historySize > 0) {
    opts.historySize = historySize;
  } else {
    opts.historySize = 1000;
  }

  const repl = StableREPL.start(opts);
  const term = 'terminal' in opts ? opts.terminal : process.stdout.isTTY;
  repl.setupHistory(term ? env.NODE_REPL_HISTORY : '', cb);
}

async function createExperimentalRepl() {
  const repl = new ExperimentalREPL();
  await repl.start();
  ExperimentalREPL.repl = repl; // For introspection
}
