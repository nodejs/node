'use strict';

const {
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
  isMainThread,
  MessagePort,
  MessageChannel,
  moveMessagePortToContext,
  threadId,
  Worker,
  parentPort: null,
  workerData: null,
};
