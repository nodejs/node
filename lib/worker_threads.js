'use strict';

const {
  drainMessagePort,
  isMainThread,
  threadId,
  Worker
} = require('internal/worker');

const {
  MessagePort,
  MessageChannel,
  moveMessagePortToContext,
} = require('internal/worker/io');

module.exports = {
  drainMessagePort,
  isMainThread,
  MessagePort,
  MessageChannel,
  moveMessagePortToContext,
  threadId,
  Worker,
  parentPort: null,
  workerData: null,
};
