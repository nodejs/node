'use strict';

function handleProcessExit() {
  if (process.exitCode === undefined)
    process.exitCode = 13;
}

module.exports = {
  handleProcessExit,
};
