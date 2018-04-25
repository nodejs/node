'use strict';

function eos(stream, opts, callback) {
  // This is for backwards compat.
  if (typeof opts === 'function') {
    callback = opts;
  }

  stream.on('error', onComplete);
  stream.on('close', onComplete);

  const cleanup = () => {
    stream.removeListener('error', onComplete);
    stream.removeListener('close', onComplete);
  };

  const onComplete = (err) => {
    cleanup();
    callback.call(stream, err);
  };

  return cleanup;
}

module.exports = eos;
