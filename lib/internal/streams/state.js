'use strict';

const { Math } = primordials;

const { ERR_INVALID_OPT_VALUE } = require('internal/errors').codes;

function highWaterMarkFrom(options, isDuplex, duplexKey) {
  return options.highWaterMark != null ? options.highWaterMark :
    isDuplex ? options[duplexKey] : null;
}

function getDefaultHighWaterMark(objectMode) {
  return objectMode ? 16 : 16 * 1024;
}

function getHighWaterMark(state, options, duplexKey, isDuplex) {
  const hwm = highWaterMarkFrom(options, isDuplex, duplexKey);
  if (hwm != null) {
    if (!Number.isInteger(hwm) || hwm < 0) {
      const name = isDuplex ? duplexKey : 'highWaterMark';
      throw new ERR_INVALID_OPT_VALUE(name, hwm);
    }
    return Math.floor(hwm);
  }

  // Default value
  return getDefaultHighWaterMark(state.objectMode);
}

function pendingWaterMarkFrom(options, isDuplex, duplexKey) {
  return options.pendingWaterMark != null ? options.pendingWaterMark :
    isDuplex ? options[duplexKey] : null;
}

function getDefaultPendingWaterMark(objectMode) {
  return objectMode ? 16 : 1024;
}

function getPendingWaterMark(state, options, duplexKey, isDuplex) {
  const pwm = pendingWaterMarkFrom(options, isDuplex, duplexKey);
  if (pwm != null) {
    if (!Number.isInteger(pwm) || pwm < 0) {
      const name = isDuplex ? duplexKey : 'pendingWaterMark';
      throw new ERR_INVALID_OPT_VALUE(name, pwm);
    }
    return Math.floor(pwm);
  }

  // Default value
  return getDefaultPendingWaterMark(state.objectMode);
}

module.exports = {
  getHighWaterMark,
  getPendingWaterMark,
  getDefaultHighWaterMark,
  getDefaultPendingWaterMark
};
