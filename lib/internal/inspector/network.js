'use strict';

const {
  NumberMAX_SAFE_INTEGER,
  Symbol,
} = primordials;

const { now } = require('internal/perf/utils');
const kInspectorRequestId = Symbol('kInspectorRequestId');

/**
 * Return a monotonically increasing time in seconds since an arbitrary point in the past.
 * @returns {number}
 */
function getMonotonicTime() {
  return now() / 1000;
}

let requestId = 0;
function getNextRequestId() {
  if (requestId === NumberMAX_SAFE_INTEGER) {
    requestId = 0;
  }
  return `node-network-event-${++requestId}`;
};

module.exports = {
  kInspectorRequestId,
  getMonotonicTime,
  getNextRequestId,
};
