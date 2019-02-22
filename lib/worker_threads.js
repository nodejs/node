'use strict';

const {
  isMainThread,
  SHARE_ENV,
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
  SHARE_ENV,
  Worker,
  parentPort: null,
  workerData: null,
};
