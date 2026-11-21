'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePop,
  Promise,
} = primordials;

const {
  isIterable,
  isNodeStream,
  isWebStream,
} = require('internal/streams/utils');

const { pipelineImpl: pl } = require('internal/streams/pipeline');
const { finished } = require('internal/streams/end-of-stream');

require('stream');

function pipeline(...streams) {
  return new Promise((resolve, reject) => {
    let signal;
    let end;
    const lastArg = streams[streams.length - 1];
    if (lastArg && typeof lastArg === 'object' &&
        !isNodeStream(lastArg) && !isIterable(lastArg) && !isWebStream(lastArg)) {
      const options = ArrayPrototypePop(streams);
      signal = options.signal;
      end = options.end;
    }

    let lastStream = streams[streams.length - 1];
    if (streams.length === 1 && ArrayIsArray(streams[0])) {
      lastStream = streams[0][streams[0].length - 1];
    }

    const stream = pl(streams, (err, value) => {
      if (err) {
        reject(err);
      } else {
        resolve(value);
      }
    }, { signal, end });

    if (typeof lastStream === 'function' && stream.readable) {
      stream.resume();
    }
  });
}

module.exports = {
  finished,
  pipeline,
};
