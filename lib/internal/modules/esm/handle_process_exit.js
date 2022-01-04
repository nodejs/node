'use strict';

// Handle a Promise from running code that potentially does Top-Level Await.
// In that case, it makes sense to set the exit code to a specific non-zero
// value if the main code never finishes running.
// The original purpose was https://github.com/nodejs/node/pull/34640
function handleProcessExit() {
  process.exitCode ??= 13;
}

module.exports = {
  handleProcessExit,
};
