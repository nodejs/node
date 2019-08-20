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
  receiveMessageOnPort
} = require('internal/worker/io');

module.exports = {
  isMainThread,
  MessagePort,
  MessageChannel,
  moveMessagePortToContext,
  receiveMessageOnPort,
  threadId,
  SHARE_ENV,
  Worker,
  parentPort: null,
  workerData: null,
};
