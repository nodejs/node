'use strict';

let internalTTy;
function lazyInternalTTY() {
  internalTTy ??= require('internal/tty');
  return internalTTy;
}

let testContext;
function getTestContext() {
  if (testContext === undefined) {
    const raw = process.env.NODE_TEST_CONTEXT;
    if (raw !== undefined && raw[0] === '{') {
      try {
        // NODE_TEST_CONTEXT is a JSON object when set by the test runner's
        // process isolation mode. We read it lazily and cache the result.
        testContext = JSON.parse(raw);
      } catch {
        testContext = null;
      }
    } else {
      testContext = null;
    }
  }
  return testContext;
}

module.exports = {
  blue: '',
  green: '',
  white: '',
  red: '',
  gray: '',
  clear: '',
  reset: '',
  hasColors: false,
  shouldColorize(stream) {
    // Process-level FORCE_COLOR has the highest priority.
    if (process.env.FORCE_COLOR !== undefined) {
      return lazyInternalTTY().getColorDepth() > 2;
    }
    // The stream's own isTTY capability is checked next.
    if (stream?.isTTY) {
      return typeof stream.getColorDepth === 'function' ?
        stream.getColorDepth() > 2 : true;
    }
    // When running as a test-runner child process, use the parent's colorize
    // decision (encoded in NODE_TEST_CONTEXT) as a last resort. This avoids
    // injecting FORCE_COLOR into the child, which would override the user's
    // explicit stream.isTTY=false checks in util.styleText().
    // See https://github.com/nodejs/node/issues/57921.
    const ctx = getTestContext();
    return ctx?.colorize === true && lazyInternalTTY().getColorDepth() > 2;
  },
  refresh() {
    if (module.exports.shouldColorize(process.stderr)) {
      module.exports.blue = '\u001b[34m';
      module.exports.green = '\u001b[32m';
      module.exports.white = '\u001b[39m';
      module.exports.yellow = '\u001b[33m';
      module.exports.red = '\u001b[31m';
      module.exports.gray = '\u001b[90m';
      module.exports.clear = '\u001bc';
      module.exports.reset = '\u001b[0m';
      module.exports.hasColors = true;
    } else {
      module.exports.blue = '';
      module.exports.green = '';
      module.exports.white = '';
      module.exports.yellow = '';
      module.exports.red = '';
      module.exports.gray = '';
      module.exports.clear = '';
      module.exports.reset = '';
      module.exports.hasColors = false;
    }
  },
};

module.exports.refresh();
