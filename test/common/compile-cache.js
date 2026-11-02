'use strict';

const common = require('./');
const childProcess = require('./child_process');

// The compile cache is disabled while V8 coverage is being collected. In that
// case, check that the first child says so and skip the rest of the test.
function withCompileCache(spawn) {
  return (...args) => {
    if (!process.env.NODE_V8_COVERAGE) {
      return spawn(...args);
    }
    args.pop();
    childProcess.spawnSyncAndExit(...args, {
      stderr: /Disabled because V8 coverage is being collected/,
    });
    common.skip('the compile cache is disabled by V8 coverage');
  };
}

module.exports = {
  spawnSyncAndAssert: withCompileCache(childProcess.spawnSyncAndAssert),
  spawnSyncAndExit: withCompileCache(childProcess.spawnSyncAndExit),
};
