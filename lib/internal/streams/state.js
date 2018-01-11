'use strict';

const stream = require('stream');
const errors = require('internal/errors');

function getHighWaterMarkFromProperty(hwm, name) {
  if (typeof hwm !== 'number' || !(hwm >= 0))
    throw new errors.TypeError('ERR_INVALID_OPT_VALUE', name, hwm);
  return Math.floor(hwm);
}

function getHighWaterMark(state, options, isDuplex) {
  if (options.highWaterMark != null) {
    return getHighWaterMarkFromProperty(options.highWaterMark, 'highWaterMark');
  } else if (isDuplex) {
    if (state instanceof stream.Readable.ReadableState) {
      if (options.readableHighWaterMark != null) {
        return getHighWaterMarkFromProperty(options.readableHighWaterMark,
                                            'readableHighWaterMark');
      }
    } else if (options.writableHighWaterMark != null) {
      return getHighWaterMarkFromProperty(options.writableHighWaterMark,
                                          'writableHighWaterMark');
    }
  }

  // Default value
  return state.objectMode ? 16 : 16 * 1024;
}

module.exports = {
  getHighWaterMark
};
