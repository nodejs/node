'use strict';

const {
  MathFloor,
  NumberIsInteger,
} = primordials;
const { validateInteger } = require('internal/validators');

const { ERR_INVALID_ARG_VALUE } = require('internal/errors').codes;

// TODO (fix): For some reason Windows CI fails with bigger hwm.
let defaultHighWaterMarkBytes = process.platform === 'win32' ? 16 * 1024 : 64 * 1024;
let defaultHighWaterMarkObjectMode = 16;
let defaultHighWaterMarkBytesSet = false;

function highWaterMarkFrom(options, isDuplex, duplexKey) {
  return options.highWaterMark != null ? options.highWaterMark :
    isDuplex ? options[duplexKey] : null;
}

function getDefaultHighWaterMark(objectMode) {
  return objectMode ? defaultHighWaterMarkObjectMode : defaultHighWaterMarkBytes;
}

function setDefaultHighWaterMark(objectMode, value) {
  validateInteger(value, 'value', 0);
  if (objectMode) {
    defaultHighWaterMarkObjectMode = value;
  } else {
    defaultHighWaterMarkBytes = value;
    defaultHighWaterMarkBytesSet = true;
  }
}

function getUserDefaultHighWaterMark() {
  return defaultHighWaterMarkBytesSet ? defaultHighWaterMarkBytes : undefined;
}

function validateHighWaterMark(value, name) {
  if (!NumberIsInteger(value) || value < 0) {
    throw new ERR_INVALID_ARG_VALUE(name, value);
  }
  return MathFloor(value);
}

function getHighWaterMark(state, options, duplexKey, isDuplex) {
  const hwm = highWaterMarkFrom(options, isDuplex, duplexKey);
  if (hwm != null) {
    const name = isDuplex ? `options.${duplexKey}` : 'options.highWaterMark';
    return validateHighWaterMark(hwm, name);
  }

  // Default value
  return getDefaultHighWaterMark(state.objectMode);
}

module.exports = {
  getHighWaterMark,
  getDefaultHighWaterMark,
  getUserDefaultHighWaterMark,
  setDefaultHighWaterMark,
  validateHighWaterMark,
};
