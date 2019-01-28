'use strict';

const {
  isMainThread,
  threadId,
  Worker
} = require('internal/worker');

const {
  MessagePort,
  MessageChannel
} = require('internal/worker/io');

module.exports = {
  isMainThread,
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  parentPort: null,
  workerData: null,
};
