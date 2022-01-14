'use strict';

// Handle a Promise from running code that potentially does Top-Level Await.
// In that case, it makes sense to set the exit code to a specific non-zero
// value if the main code never finishes running.
function handleProcessExit() {
  process.exitCode ??= 13;
}

module.exports = {
  handleProcessExit,
};
