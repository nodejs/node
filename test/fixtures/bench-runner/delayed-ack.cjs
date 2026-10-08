'use strict';

const common = require('../../common');
const childProcess = require('child_process');
const { spawn } = childProcess;

childProcess.spawn = (...args) => {
  const child = spawn(...args);
  const { send } = child;
  child.send = function(message, handle, options, callback) {
    if (message?.type === 'node:bench:ack') {
      // Report send completion only after the child has responded to the ack.
      const onComplete = common.mustCall(() => {
        child.removeListener('message', onComplete);
        child.removeListener('close', onComplete);
        callback(null);
      });
      child.once('message', onComplete);
      child.once('close', onComplete);
      return send.call(this, message, handle, options, (error) => {
        if (error) {
          child.removeListener('message', onComplete);
          child.removeListener('close', onComplete);
          callback(error);
        }
      });
    }
    return send.call(this, message, handle, options, callback);
  };
  return child;
};
