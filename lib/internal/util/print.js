'use strict';

// This implements a light-weight printer that writes to stdout/stderr
// directly to avoid the overhead in the console abstraction.

const { formatWithOptions } = require('internal/util/inspect');
const { writeString } = internalBinding('fs');
const { handleErrorFromBinding } = require('internal/fs/utils');
const { guessHandleType } = internalBinding('util');
const { log } = require('internal/console/global');

const kStdout = 1;
const kStderr = 2;
const handleType = [undefined, undefined, undefined];
function getFdType(fd) {
  if (handleType[fd] === undefined) {
    handleType[fd] = guessHandleType(fd);
  }
  return handleType[fd];
}

function formatAndWrite(fd, obj, ignoreErrors, colors = false) {
  const str = `${formatWithOptions({ colors }, obj)}\n`;
  const ctx = {};
  writeString(fd, str, null, undefined, undefined, ctx);
  if (!ignoreErrors) {
    handleErrorFromBinding(ctx);
  }
}

let colors;
function getColors() {
  if (colors === undefined) {
    colors = require('internal/tty').getColorDepth() > 2;
  }
  return colors;
}

// TODO(joyeecheung): replace more internal process._rawDebug()
// and console.log() usage with this if possible.
function print(fd, obj, ignoreErrors = true) {
  switch (getFdType(fd)) {
    case 'TTY':
      formatAndWrite(fd, obj, ignoreErrors, getColors());
      break;
    case 'FILE':
      formatAndWrite(fd, obj, ignoreErrors);
      break;
    case 'PIPE':
    case 'TCP':
      // Fallback to console.log to handle IPC.
      if (process.channel && process.channel.fd === fd) {
        log(obj);
      } else {
        formatAndWrite(fd, obj, ignoreErrors);
      }
      break;
    default:
      log(obj);
  }
}

module.exports = {
  print,
  kStderr,
  kStdout
};
