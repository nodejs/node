'use strict';

function handleProcessExit() {
  process.exitCode ??= 13;
}

module.exports = {
  handleProcessExit,
};
