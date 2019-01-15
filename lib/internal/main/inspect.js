'use strict';

// `node inspect ...` or `node debug ...`

if (process.argv[1] === 'debug') {
  process.emitWarning(
    '`node debug` is deprecated. Please use `node inspect` instead.',
    'DeprecationWarning', 'DEP0068');
}

markBootstrapComplete();

// Start the debugger agent.
process.nextTick(() => {
  require('internal/deps/node-inspect/lib/_inspect').start();
});
